#include "mini_lexical_search/document.hpp"

#include <charconv>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <unordered_set>

namespace mini_lexical_search {
namespace {

// Strict parser for the Document array contract, including JSON string escapes.
class DocumentParser {
public:
    explicit DocumentParser(std::string text) : text_(std::move(text)) {}

    std::vector<Document> parse() {
        std::vector<Document> documents;
        std::unordered_set<std::string> paths;
        std::unordered_set<DocumentId> ids;
        expect('[');
        if (!take(']')) {
            do {
                auto document = object();
                if (!paths.insert(document.path).second || !ids.insert(document.id).second) {
                    fail("duplicate document path or ID");
                }
                documents.push_back(std::move(document));
            } while (take(','));
            expect(']');
        }
        whitespace();
        if (position_ != text_.size()) {
            fail("trailing data");
        }
        return documents;
    }

private:
    [[noreturn]] void fail(const std::string& reason) const {
        throw std::runtime_error("invalid document JSON at byte " +
                                 std::to_string(position_) + ": " + reason);
    }

    void whitespace() {
        while (position_ < text_.size() &&
               std::string(" \t\r\n").find(text_[position_]) != std::string::npos) {
            ++position_;
        }
    }

    bool take(char value) {
        whitespace();
        if (position_ < text_.size() && text_[position_] == value) {
            ++position_;
            return true;
        }
        return false;
    }

    void expect(char value) {
        if (!take(value)) {
            fail(std::string("expected '") + value + "'");
        }
    }

    unsigned int hexCode() {
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

    void unicode(std::string& value) {
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

    std::string string() {
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

    std::uint32_t number() {
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

    Document object() {
        Document document{};
        std::unordered_set<std::string> fields;
        expect('{');
        do {
            const auto key = string();
            if (!fields.insert(key).second) fail("duplicate field: " + key);
            expect(':');
            if (key == "id") document.id = number();
            else if (key == "path") document.path = string();
            else if (key == "title") document.title = string();
            else if (key == "tokenCount") document.tokenCount = number();
            else if (key == "hash") document.hash = string();
            else fail("unknown field: " + key);
        } while (take(','));
        expect('}');
        for (const auto* required : {"id", "path", "title", "tokenCount"}) {
            if (!fields.count(required)) fail(std::string("missing field: ") + required);
        }
        if (document.path.empty()) fail("empty document path");
        if (!document.hash.empty() &&
            (document.hash.size() != 64 ||
             document.hash.find_first_not_of("0123456789abcdef") != std::string::npos)) {
            fail("invalid SHA-256 hash");
        }
        return document;
    }

    std::string text_;
    std::size_t position_ = 0;
};

}  // namespace

std::vector<Document> loadDocuments(const std::filesystem::path& input) {
    std::ifstream stream(input, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot read document JSON: " + input.string());
    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) throw std::runtime_error("cannot read document JSON: " + input.string());
    return DocumentParser(std::move(text)).parse();
}

}  // namespace mini_lexical_search
