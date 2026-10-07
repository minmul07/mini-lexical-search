#include "mini_lexical_search/indexer.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "json_internal.hpp"

namespace mini_lexical_search {
namespace {

std::vector<std::uint32_t> parseLines(json::Reader& reader) {
    std::vector<std::uint32_t> lines;
    reader.expect('[');
    if (!reader.take(']')) {
        do {
            lines.push_back(reader.unsignedInteger());
        } while (reader.take(','));
        reader.expect(']');
    }
    return lines;
}

Posting parsePosting(json::Reader& reader) {
    Posting posting;
    std::unordered_set<std::string> fields;
    reader.expect('{');
    do {
        const auto key = reader.string();
        if (!fields.insert(key).second) reader.fail("duplicate field: " + key);
        reader.expect(':');
        if (key == "documentId") posting.documentId = reader.unsignedInteger();
        else if (key == "termFrequency") posting.termFrequency = reader.unsignedInteger();
        else if (key == "lines") posting.lines = parseLines(reader);
        else reader.fail("unknown field: " + key);
    } while (reader.take(','));
    reader.expect('}');
    for (const auto* required : {"documentId", "termFrequency", "lines"}) {
        if (!fields.count(required)) reader.fail(std::string("missing field: ") + required);
    }
    return posting;
}

PostingList parsePostingList(json::Reader& reader) {
    PostingList postings;
    reader.expect('[');
    if (!reader.take(']')) {
        do {
            postings.push_back(parsePosting(reader));
        } while (reader.take(','));
        reader.expect(']');
    }
    return postings;
}

std::unordered_map<std::string, PostingList> parseInvertedIndex(json::Reader& reader) {
    std::unordered_map<std::string, PostingList> postings;
    reader.expect('{');
    if (!reader.take('}')) {
        do {
            const auto term = reader.string();
            reader.expect(':');
            auto list = parsePostingList(reader);
            if (!postings.emplace(term, std::move(list)).second) {
                reader.fail("duplicate term: " + term);
            }
        } while (reader.take(','));
        reader.expect('}');
    }
    return postings;
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
    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        throw std::runtime_error("cannot read index snapshot: " + input.string());
    }

    json::Reader reader(std::move(text), "index");
    IndexSnapshot snapshot;
    bool hasVersion = false;
    bool hasDocuments = false;
    bool hasIndex = false;
    std::uint32_t version = 0;
    reader.expect('{');
    if (!reader.take('}')) {
        do {
            const auto key = reader.string();
            reader.expect(':');
            if (key == "version") {
                if (hasVersion) reader.fail("duplicate field: version");
                version = reader.unsignedInteger();
                hasVersion = true;
            } else if (key == "documents") {
                if (hasDocuments) reader.fail("duplicate field: documents");
                snapshot.documents = json::parseDocumentArray(reader);
                hasDocuments = true;
            } else if (key == "invertedIndex") {
                if (hasIndex) reader.fail("duplicate field: invertedIndex");
                snapshot.index = InvertedIndex::fromPostings(parseInvertedIndex(reader));
                hasIndex = true;
            } else {
                reader.fail("unknown field: " + key);
            }
        } while (reader.take(','));
        reader.expect('}');
    }
    reader.finish();
    if (!hasVersion || !hasDocuments || !hasIndex) {
        reader.fail("missing fields");
    }
    if (version != 1) {
        reader.fail("unsupported version: " + std::to_string(version));
    }
    std::unordered_set<DocumentId> ids;
    for (const auto& document : snapshot.documents) {
        ids.insert(document.id);
    }
    for (const auto& entry : snapshot.index.postings()) {
        for (const auto& posting : entry.second) {
            if (!ids.count(posting.documentId)) {
                reader.fail("posting references unknown document");
            }
        }
    }
    return snapshot;
}

void saveIndexSnapshot(const std::vector<Document>& documents,
                       const InvertedIndex& index,
                       const std::filesystem::path& output) {
    std::vector<std::string> terms;
    terms.reserve(index.termCount());
    for (const auto& entry : index.postings()) {
        terms.push_back(entry.first);
    }
    std::sort(terms.begin(), terms.end());

    json::writeFileAtomically(output, [&](std::ostream& stream) {
        stream << "{\n  \"version\": 1,\n  \"documents\": ";
        if (documents.empty()) {
            stream << "[]";
        } else {
            stream << "[\n";
            for (std::size_t i = 0; i < documents.size(); ++i) {
                stream << "    ";
                json::writeDocument(stream, documents[i]);
                stream << (i + 1 < documents.size() ? ",\n" : "\n");
            }
            stream << "  ]";
        }
        stream << ",\n  \"invertedIndex\": ";
        if (terms.empty()) {
            stream << "{}";
        } else {
            stream << "{\n";
            for (std::size_t i = 0; i < terms.size(); ++i) {
                const auto* postings = index.find(terms[i]);
                stream << "    ";
                json::writeString(stream, terms[i]);
                stream << ": [";
                for (std::size_t j = 0; j < postings->size(); ++j) {
                    const auto& posting = (*postings)[j];
                    if (j > 0) stream << ", ";
                    stream << "{\"documentId\": " << posting.documentId
                           << ", \"termFrequency\": " << posting.termFrequency
                           << ", \"lines\": [";
                    for (std::size_t k = 0; k < posting.lines.size(); ++k) {
                        if (k > 0) stream << ", ";
                        stream << posting.lines[k];
                    }
                    stream << "]}";
                }
                stream << (i + 1 < terms.size() ? "],\n" : "]\n");
            }
            stream << "  }";
        }
        stream << "\n}\n";
    });
}

}  // namespace mini_lexical_search
