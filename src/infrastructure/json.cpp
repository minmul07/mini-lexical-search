#include "json_internal.hpp"

#include <charconv>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace mini_lexical_search::json {

Reader::Reader(std::string text, std::string label)
    : text_(std::move(text)), label_(std::move(label)) {}

[[noreturn]] void Reader::fail(const std::string& reason) const {
    throw std::runtime_error("invalid " + label_ + " JSON at byte " +
                             std::to_string(position_) + ": " + reason);
}

void Reader::whitespace() {
    while (position_ < text_.size() &&
           std::string(" \t\r\n").find(text_[position_]) != std::string::npos) {
        ++position_;
    }
}

bool Reader::take(char value) {
    whitespace();
    if (position_ < text_.size() && text_[position_] == value) {
        ++position_;
        return true;
    }
    return false;
}

void Reader::expect(char value) {
    if (!take(value)) {
        fail(std::string("expected '") + value + "'");
    }
}

unsigned int Reader::hexCode() {
    unsigned int code = 0;
    for (int i = 0; i < 4; ++i) {
        if (position_ == text_.size()) {
            fail("incomplete unicode escape");
        }
        const char digit = text_[position_++];
        code <<= 4;
        if (digit >= '0' && digit <= '9') code += digit - '0';
        else if (digit >= 'a' && digit <= 'f') code += digit - 'a' + 10;
        else if (digit >= 'A' && digit <= 'F') code += digit - 'A' + 10;
        else fail("invalid unicode escape");
    }
    return code;
}

void Reader::unicode(std::string& value) {
    auto code = hexCode();
    if (code >= 0xd800 && code <= 0xdbff) {
        if (text_.compare(position_, 2, "\\u") != 0) {
            fail("missing low surrogate");
        }
        position_ += 2;
        const auto low = hexCode();
        if (low < 0xdc00 || low > 0xdfff) fail("invalid low surrogate");
        code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
    } else if (code >= 0xdc00 && code <= 0xdfff) {
        fail("unpaired low surrogate");
    }
    if (code < 0x80) {
        value += static_cast<char>(code);
    } else if (code < 0x800) {
        value += static_cast<char>(0xc0 | (code >> 6));
        value += static_cast<char>(0x80 | (code & 0x3f));
    } else if (code < 0x10000) {
        value += static_cast<char>(0xe0 | (code >> 12));
        value += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        value += static_cast<char>(0x80 | (code & 0x3f));
    } else {
        value += static_cast<char>(0xf0 | (code >> 18));
        value += static_cast<char>(0x80 | ((code >> 12) & 0x3f));
        value += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
        value += static_cast<char>(0x80 | (code & 0x3f));
    }
}

std::string Reader::string() {
    expect('"');
    std::string value;
    while (position_ < text_.size()) {
        const unsigned char character = text_[position_++];
        if (character == '"') return value;
        if (character < 0x20) fail("unescaped control character");
        if (character != '\\') {
            value += static_cast<char>(character);
            continue;
        }
        if (position_ == text_.size()) fail("incomplete escape");
        switch (text_[position_++]) {
            case '"': value += '"'; break;
            case '\\': value += '\\'; break;
            case '/': value += '/'; break;
            case 'b': value += '\b'; break;
            case 'f': value += '\f'; break;
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            case 'u': unicode(value); break;
            default: fail("invalid escape");
        }
    }
    fail("unterminated string");
}

std::uint32_t Reader::unsignedInteger() {
    whitespace();
    const auto start = position_;
    while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
        ++position_;
    }
    if (position_ == start || (position_ - start > 1 && text_[start] == '0')) {
        fail("expected unsigned integer");
    }
    std::uint32_t value = 0;
    const auto result = std::from_chars(text_.data() + start, text_.data() + position_, value);
    if (result.ec != std::errc{}) fail("integer out of range");
    return value;
}

void Reader::finish() {
    whitespace();
    if (position_ != text_.size()) {
        fail("trailing data");
    }
}

void writeString(std::ostream& output, const std::string& value) {
    constexpr char hex[] = "0123456789abcdef";
    output << '"';
    for (unsigned char character : value) {
        if (character == '"' || character == '\\') {
            output << '\\' << static_cast<char>(character);
        } else if (character < 0x20) {
            output << "\\u00" << hex[character >> 4] << hex[character & 0xf];
        } else {
            output << static_cast<char>(character);
        }
    }
    output << '"';
}

void writeDocument(std::ostream& output, const Document& document) {
    output << "{\"id\": " << document.id << ", \"path\": ";
    writeString(output, document.path);
    output << ", \"title\": ";
    writeString(output, document.title);
    output << ", \"tokenCount\": " << document.tokenCount << ", \"hash\": ";
    writeString(output, document.hash);
    output << '}';
}

Document parseDocument(Reader& reader) {
    Document document{};
    std::unordered_set<std::string> fields;
    reader.expect('{');
    do {
        const auto key = reader.string();
        if (!fields.insert(key).second) reader.fail("duplicate field: " + key);
        reader.expect(':');
        if (key == "id") document.id = reader.unsignedInteger();
        else if (key == "path") document.path = reader.string();
        else if (key == "title") document.title = reader.string();
        else if (key == "tokenCount") document.tokenCount = reader.unsignedInteger();
        else if (key == "hash") document.hash = reader.string();
        else reader.fail("unknown field: " + key);
    } while (reader.take(','));
    reader.expect('}');
    for (const auto* required : {"id", "path", "title", "tokenCount"}) {
        if (!fields.count(required)) reader.fail(std::string("missing field: ") + required);
    }
    if (document.path.empty()) reader.fail("empty document path");
    if (!document.hash.empty() &&
        (document.hash.size() != 64 ||
         document.hash.find_first_not_of("0123456789abcdef") != std::string::npos)) {
        reader.fail("invalid SHA-256 hash");
    }
    return document;
}

std::vector<Document> parseDocumentArray(Reader& reader) {
    std::vector<Document> documents;
    std::unordered_set<std::string> paths;
    std::unordered_set<DocumentId> ids;
    reader.expect('[');
    if (!reader.take(']')) {
        do {
            auto document = parseDocument(reader);
            if (!paths.insert(document.path).second || !ids.insert(document.id).second) {
                reader.fail("duplicate document path or ID");
            }
            documents.push_back(std::move(document));
        } while (reader.take(','));
        reader.expect(']');
    }
    return documents;
}

void writeFileAtomically(const std::filesystem::path& output,
                         const std::function<void(std::ostream&)>& write) {
    if (!output.parent_path().empty()) {
        std::filesystem::create_directories(output.parent_path());
    }
    auto temporary = output;
    temporary += ".tmp";
    // Reserve the staging directory so concurrent writers cannot share a file.
    if (!std::filesystem::create_directory(temporary)) {
        throw std::runtime_error("temporary directory already exists: " + temporary.string());
    }
    const auto stagedFile = temporary / output.filename();
    try {
        std::ofstream stream;
        stream.exceptions(std::ios::failbit | std::ios::badbit);
        stream.open(stagedFile, std::ios::binary | std::ios::trunc);
        write(stream);
        stream.close();
        std::filesystem::rename(stagedFile, output);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(stagedFile, ignored);
        std::filesystem::remove(temporary, ignored);
        throw;
    }
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
}

}  // namespace mini_lexical_search::json
