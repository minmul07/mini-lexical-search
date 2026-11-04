#include "mini_lexical_search/infrastructure/index_storage.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "json_internal.hpp"
#include "mini_lexical_search/infrastructure/sha256.hpp"

namespace mini_lexical_search {
namespace {

constexpr std::string_view kMagic = "MLSI";
constexpr std::uint32_t kFormatVersion = 4;
constexpr std::size_t kDigestSize = 32;
constexpr std::size_t kU32Size = 4;
constexpr std::size_t kDocumentHeaderSize = 5;
constexpr std::size_t kTermHeaderSize = 2;
constexpr std::size_t kPostingHeaderSize = 3;

void writeU8(std::ostream& output, std::uint8_t value) {
    output.put(static_cast<char>(value));
}

void writeU32(std::ostream& output, std::uint32_t value) {
    const std::array<char, 4> bytes{
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8) & 0xffu),
        static_cast<char>((value >> 16) & 0xffu),
        static_cast<char>((value >> 24) & 0xffu),
    };
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// unsigned Base-128: 하위 7비트부터 기록한다.
void writeVarUint32(std::ostream& output, std::uint32_t value) {
    while (value >= 0x80u) {
        writeU8(output, static_cast<std::uint8_t>((value & 0x7fu) | 0x80u));
        value >>= 7;
    }
    writeU8(output, static_cast<std::uint8_t>(value));
}

void writeCount(std::ostream& output, std::size_t count, const char* label) {
    if (count > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(std::string("index snapshot ") + label + " count is too large");
    }
    writeVarUint32(output, static_cast<std::uint32_t>(count));
}

void writeString(std::ostream& output, const std::string& value) {
    writeCount(output, value.size(), "string");
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

std::uint8_t hexDigit(char character) {
    if (character >= '0' && character <= '9') {
        return static_cast<std::uint8_t>(character - '0');
    }
    if (character >= 'a' && character <= 'f') {
        return static_cast<std::uint8_t>(character - 'a' + 10);
    }
    throw std::runtime_error("invalid document hash");
}

std::array<std::uint8_t, kDigestSize> decodeHash(const std::string& hash) {
    if (hash.size() != kDigestSize * 2) {
        throw std::runtime_error("invalid document hash");
    }
    std::array<std::uint8_t, kDigestSize> digest{};
    for (std::size_t i = 0; i < digest.size(); ++i) {
        digest[i] = static_cast<std::uint8_t>((hexDigit(hash[i * 2]) << 4) |
                                              hexDigit(hash[i * 2 + 1]));
    }
    return digest;
}

class BinaryReader {
public:
    explicit BinaryReader(std::string data) : data_(std::move(data)) {}

    std::size_t remaining() const { return data_.size() - position_; }

    void expectMagic(std::string_view magic) {
        require(magic.size());
        if (std::string_view(data_).substr(position_, magic.size()) != magic) {
            fail("invalid magic");
        }
        position_ += magic.size();
    }

    std::uint8_t u8() {
        require(1);
        return static_cast<std::uint8_t>(data_[position_++]);
    }

    std::uint32_t u32() {
        require(kU32Size);
        const auto byte = [this](std::size_t offset) {
            return static_cast<std::uint32_t>(
                static_cast<std::uint8_t>(data_[position_ + offset]));
        };
        const std::uint32_t value =
            byte(0) | (byte(1) << 8) | (byte(2) << 16) | (byte(3) << 24);
        position_ += kU32Size;
        return value;
    }

    std::uint32_t varUint32() {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 5; ++i) {
            const auto byte = u8();
            if (i == 4 && (byte & 0xf0u)) fail("varint overflow");
            value |= static_cast<std::uint32_t>(byte & 0x7fu) << (7 * i);
            if (!(byte & 0x80u)) {
                if (i != 0 && byte == 0) fail("non-canonical varint");
                return value;
            }
        }
        fail("unterminated varint");
    }

    // 첫 문서 ID만 0을 허용하며 덧셈 전에 오버플로를 검사한다.
    std::uint32_t delta(std::uint32_t previous, bool allowZero) {
        const auto gap = varUint32();
        if (!allowZero && gap == 0) fail("zero delta");
        if (gap > std::numeric_limits<std::uint32_t>::max() - previous) {
            fail("delta overflow");
        }
        return previous + gap;
    }

    std::string string() {
        const auto length = static_cast<std::size_t>(varUint32());
        if (length > remaining()) fail("truncated string");
        std::string value = data_.substr(position_, length);
        position_ += length;
        return value;
    }

    std::array<std::uint8_t, kDigestSize> digest() {
        require(kDigestSize);
        std::array<std::uint8_t, kDigestSize> digest{};
        for (std::size_t i = 0; i < digest.size(); ++i) {
            digest[i] = static_cast<std::uint8_t>(data_[position_ + i]);
        }
        position_ += digest.size();
        return digest;
    }

    std::size_t boundedCount(std::uint32_t count,
                             std::size_t minimumSize,
                             const char* label) const {
        if (minimumSize != 0 && static_cast<std::size_t>(count) > remaining() / minimumSize) {
            fail(std::string("invalid ") + label + " count");
        }
        return static_cast<std::size_t>(count);
    }

    void finish() const {
        if (remaining() != 0) fail("trailing data");
    }

    [[noreturn]] void fail(const std::string& reason) const {
        throw std::runtime_error("invalid index binary at byte " + std::to_string(position_) +
                                 ": " + reason);
    }

private:
    void require(std::size_t size) const {
        if (remaining() < size) fail("unexpected end of file");
    }

    std::string data_;
    std::size_t position_ = 0;
};

void writeSnapshot(std::ostream& output,
                   const IndexSnapshot& snapshot) {
    output.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
    writeU32(output, kFormatVersion);
    writeString(output, snapshot.root.generic_string());
    const auto& documents = snapshot.documents;
    const auto& index = snapshot.index;
    writeCount(output, documents.size(), "document");
    for (const auto& document : documents) {
        writeVarUint32(output, document.id);
        writeString(output, document.path);
        writeString(output, document.title);
        writeVarUint32(output, document.tokenCount);
        if (document.hash.empty()) {
            writeU8(output, 0);
        } else {
            writeU8(output, static_cast<std::uint8_t>(kDigestSize));
            const auto digest = decodeHash(document.hash);
            output.write(reinterpret_cast<const char*>(digest.data()),
                         static_cast<std::streamsize>(digest.size()));
        }
    }

    std::vector<std::string> terms;
    terms.reserve(index.termCount());
    for (const auto& entry : index.postings()) {
        terms.push_back(entry.first);
    }
    std::sort(terms.begin(), terms.end());

    writeCount(output, terms.size(), "term");
    for (const auto& term : terms) {
        writeString(output, term);
        const auto* postings = index.find(term);
        if (postings == nullptr) {
            throw std::runtime_error("index snapshot term disappeared: " + term);
        }
        writeCount(output, postings->size(), "posting");
        std::uint32_t previousDocument = 0;
        bool firstDocument = true;
        for (const auto& posting : *postings) {
            if (!firstDocument && posting.documentId <= previousDocument) {
                throw std::runtime_error("invalid posting document order");
            }
            writeVarUint32(output, posting.documentId - previousDocument);
            previousDocument = posting.documentId;
            firstDocument = false;
            writeVarUint32(output, posting.termFrequency);
            writeCount(output, posting.lines.size(), "line");
            std::uint32_t previousLine = 0;
            for (const auto line : posting.lines) {
                if (line <= previousLine) throw std::runtime_error("invalid posting line order");
                writeVarUint32(output, line - previousLine);
                previousLine = line;
            }
        }
    }
}

}  // namespace

IndexSnapshot loadIndexSnapshot(const std::filesystem::path& input) {
    if (!std::filesystem::exists(input)) {
        return {};
    }
    std::ifstream stream(input, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("cannot read index snapshot: " + input.string());
    }
    std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        throw std::runtime_error("cannot read index snapshot: " + input.string());
    }

    BinaryReader reader(std::move(bytes));
    reader.expectMagic(kMagic);
    const auto version = reader.u32();
    if (version != kFormatVersion) {
        reader.fail("unsupported version: " + std::to_string(version));
    }

    IndexSnapshot snapshot;
    snapshot.root = reader.string();
    if (!snapshot.root.is_absolute() || snapshot.root != snapshot.root.lexically_normal()) {
        reader.fail("index root must be a normalized absolute path");
    }
    const auto documentCount =
        reader.boundedCount(reader.varUint32(), kDocumentHeaderSize, "document");
    snapshot.documents.reserve(documentCount);
    std::unordered_set<DocumentId> documentIds;
    std::unordered_set<std::string> documentPaths;
    documentIds.reserve(documentCount);
    documentPaths.reserve(documentCount);
    for (std::size_t i = 0; i < documentCount; ++i) {
        Document document{};
        document.id = reader.varUint32();
        document.path = reader.string();
        document.title = reader.string();
        document.tokenCount = reader.varUint32();
        const auto hashLength = reader.u8();
        if (hashLength == 0) {
            document.hash.clear();
        } else if (hashLength == kDigestSize) {
            document.hash = toHex(reader.digest());
        } else {
            reader.fail("invalid hash length");
        }
        const std::filesystem::path path(document.path);
        const auto relative = path.lexically_relative(snapshot.root);
        if (!path.is_absolute() || path != path.lexically_normal() || relative.empty() ||
            relative == "." || *relative.begin() == "..") {
            reader.fail("document path is outside index root");
        }
        if (document.path.empty()) {
            reader.fail("empty document path");
        }
        if (!documentIds.insert(document.id).second) {
            reader.fail("duplicate document ID");
        }
        if (!documentPaths.insert(document.path).second) {
            reader.fail("duplicate document path");
        }
        snapshot.documents.push_back(std::move(document));
    }

    const auto termCount = reader.boundedCount(reader.varUint32(), kTermHeaderSize, "term");
    std::unordered_map<std::string, PostingList> postings;
    postings.reserve(termCount);
    for (std::size_t i = 0; i < termCount; ++i) {
        auto term = reader.string();
        const auto postingCount =
            reader.boundedCount(reader.varUint32(), kPostingHeaderSize, "posting");
        PostingList list;
        list.reserve(postingCount);
        std::uint32_t previousDocument = 0;
        for (std::size_t j = 0; j < postingCount; ++j) {
            Posting posting;
            posting.documentId = reader.delta(previousDocument, j == 0);
            previousDocument = posting.documentId;
            posting.termFrequency = reader.varUint32();
            const auto lineCount = reader.boundedCount(reader.varUint32(), 1, "line");
            posting.lines.reserve(lineCount);
            std::uint32_t previousLine = 0;
            for (std::size_t k = 0; k < lineCount; ++k) {
                const auto line = reader.delta(previousLine, false);
                posting.lines.push_back(line);
                previousLine = line;
            }
            list.push_back(std::move(posting));
        }
        if (!postings.emplace(std::move(term), std::move(list)).second) {
            reader.fail("duplicate term");
        }
    }
    reader.finish();

    snapshot.index = InvertedIndex::fromPostings(std::move(postings));
    for (const auto& [term, list] : snapshot.index.postings()) {
        for (const auto& posting : list) {
            if (documentIds.find(posting.documentId) == documentIds.end()) {
                throw std::runtime_error("invalid index binary: posting for '" + term +
                                         "' references unknown document " +
                                         std::to_string(posting.documentId));
            }
        }
    }
    return snapshot;
}

void saveIndexSnapshot(const IndexSnapshot& snapshot,
                       const std::filesystem::path& output) {
    if (!snapshot.root.is_absolute() || snapshot.root != snapshot.root.lexically_normal()) {
        throw std::invalid_argument("index root must be a normalized absolute path");
    }
    for (const auto& document : snapshot.documents) {
        const std::filesystem::path path(document.path);
        const auto relative = path.lexically_relative(snapshot.root);
        if (!path.is_absolute() || path != path.lexically_normal() || relative.empty() ||
            relative == "." || *relative.begin() == "..") {
            throw std::invalid_argument("document path is outside index root");
        }
    }
    json::writeFileAtomically(output, [&](std::ostream& stream) {
        writeSnapshot(stream, snapshot);
    });
}

}  // namespace mini_lexical_search
