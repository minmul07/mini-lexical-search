#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

#include "mini_lexical_search/domain/document.hpp"

namespace mini_lexical_search::json {

// Cursor over a JSON text with strict primitives. `label` names the format in
// error messages, for example "document" or "index".
class Reader {
public:
    Reader(std::string text, std::string label);

    // Skips whitespace and consumes `value` when it is next.
    bool take(char value);

    void expect(char value);
    std::string string();
    std::uint32_t unsignedInteger();

    // Rejects trailing data after the parsed value.
    void finish();

    [[noreturn]] void fail(const std::string& reason) const;

private:
    void whitespace();
    unsigned int hexCode();
    void unicode(std::string& value);

    std::string text_;
    std::string label_;
    std::size_t position_ = 0;
};

// Writes a JSON string literal, escaping quotes, backslashes, and control
// characters. Bytes at or above 0x20 are emitted unchanged, so UTF-8 text
// survives the round trip.
void writeString(std::ostream& output, const std::string& value);

// Writes one Document object without indentation.
void writeDocument(std::ostream& output, const Document& document);

// Parses one `{...}` Document object.
Document parseDocument(Reader& reader);

// Parses a `[...]` array of Documents and rejects duplicate paths and IDs.
std::vector<Document> parseDocumentArray(Reader& reader);

// Writes `output` through a staging directory so a failed write keeps the
// previous file. `write` receives a stream opened for binary truncation.
void writeFileAtomically(const std::filesystem::path& output,
                         const std::function<void(std::ostream&)>& write);

}  // namespace mini_lexical_search::json
