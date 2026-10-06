#include "mini_lexical_search/tokenizer.hpp"

namespace mini_lexical_search {

std::vector<std::string> SimpleTokenizer::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string token;

    for (const unsigned char character : text) {
        if (character == ' ' || character == '\t' || character == '\n' ||
            character == '\r' || character == '\v' || character == '\f') {
            if (!token.empty()) {
                tokens.push_back(std::move(token));
                token.clear();
            }
        } else if (character >= 0x80) {
            // Preserve multi-byte UTF-8 sequences (for example Korean text).
            token += static_cast<char>(character);
        } else if (character >= '0' && character <= '9') {
            token += static_cast<char>(character);
        } else if (character >= 'A' && character <= 'Z') {
            token += static_cast<char>(character - 'A' + 'a');
        } else if (character >= 'a' && character <= 'z') {
            token += static_cast<char>(character);
        }
        // Other ASCII punctuation and control characters are dropped.
    }

    if (!token.empty()) {
        tokens.push_back(std::move(token));
    }
    return tokens;
}

}  // namespace mini_lexical_search
