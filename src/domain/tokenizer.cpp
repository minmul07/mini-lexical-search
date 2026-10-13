#include "mini_lexical_search/domain/tokenizer.hpp"

namespace mini_lexical_search {
namespace {

bool isContinuationByte(unsigned char byte) {
    return (byte & 0xC0) == 0x80;
}

// Decodes one UTF-8 code point starting at text[index]. Returns false for
// malformed input (overlong encodings, surrogates, code points above U+10FFFF,
// or truncated sequences); length is then set to 1 so the caller can skip the
// offending byte and continue.
bool decodeUtf8(const std::string& text, std::size_t index, char32_t& codePoint,
                std::size_t& length) {
    const unsigned char first = static_cast<unsigned char>(text[index]);
    const std::size_t remaining = text.size() - index;

    if (first >= 0xC2 && first <= 0xDF && remaining >= 2 &&
        isContinuationByte(static_cast<unsigned char>(text[index + 1]))) {
        codePoint = (static_cast<char32_t>(first & 0x1F) << 6) |
                    static_cast<char32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F);
        length = 2;
        return true;
    }
    if (first >= 0xE0 && first <= 0xEF && remaining >= 3) {
        const unsigned char second = static_cast<unsigned char>(text[index + 1]);
        const unsigned char third = static_cast<unsigned char>(text[index + 2]);
        const bool secondValid = isContinuationByte(second) &&
                                 (first != 0xE0 || second >= 0xA0) &&
                                 (first != 0xED || second <= 0x9F);
        if (secondValid && isContinuationByte(third)) {
            codePoint = (static_cast<char32_t>(first & 0x0F) << 12) |
                        (static_cast<char32_t>(second & 0x3F) << 6) |
                        static_cast<char32_t>(third & 0x3F);
            length = 3;
            return true;
        }
    }
    if (first >= 0xF0 && first <= 0xF4 && remaining >= 4) {
        const unsigned char second = static_cast<unsigned char>(text[index + 1]);
        const unsigned char third = static_cast<unsigned char>(text[index + 2]);
        const unsigned char fourth = static_cast<unsigned char>(text[index + 3]);
        const bool secondValid = isContinuationByte(second) &&
                                 (first != 0xF0 || second >= 0x90) &&
                                 (first != 0xF4 || second <= 0x8F);
        if (secondValid && isContinuationByte(third) && isContinuationByte(fourth)) {
            codePoint = (static_cast<char32_t>(first & 0x07) << 18) |
                        (static_cast<char32_t>(second & 0x3F) << 12) |
                        (static_cast<char32_t>(third & 0x3F) << 6) |
                        static_cast<char32_t>(fourth & 0x3F);
            length = 4;
            return true;
        }
    }

    length = 1;
    return false;
}

// Only ASCII letters/digits and Hangul code points are token characters.
// Everything else -- whitespace, punctuation, emoji, BOM, and other scripts --
// separates tokens and is excluded from the output.
bool isTokenCodePoint(char32_t codePoint) {
    return (codePoint >= '0' && codePoint <= '9') ||
           (codePoint >= 'A' && codePoint <= 'Z') ||
           (codePoint >= 'a' && codePoint <= 'z') ||
           (codePoint >= 0xAC00 && codePoint <= 0xD7A3) ||  // Hangul syllables
           (codePoint >= 0x1100 && codePoint <= 0x11FF) ||  // Hangul Jamo
           (codePoint >= 0x3130 && codePoint <= 0x318F) ||  // compatibility Jamo
           (codePoint >= 0xA960 && codePoint <= 0xA97F) ||  // Jamo Extended-A
           (codePoint >= 0xD7B0 && codePoint <= 0xD7FF);    // Jamo Extended-B
}

void flushToken(std::vector<std::string>& tokens, std::string& token) {
    if (!token.empty()) {
        tokens.push_back(std::move(token));
        token.clear();
    }
}

}  // namespace

std::vector<std::string> SimpleTokenizer::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string token;

    for (std::size_t index = 0; index < text.size();) {
        const unsigned char byte = static_cast<unsigned char>(text[index]);
        if (byte < 0x80) {
            if (byte >= '0' && byte <= '9') {
                token += static_cast<char>(byte);
            } else if (byte >= 'a' && byte <= 'z') {
                token += static_cast<char>(byte);
            } else if (byte >= 'A' && byte <= 'Z') {
                token += static_cast<char>(byte - 'A' + 'a');
            } else {
                // Whitespace, punctuation, and control bytes separate tokens.
                flushToken(tokens, token);
            }
            ++index;
            continue;
        }

        char32_t codePoint = 0;
        std::size_t length = 1;
        if (decodeUtf8(text, index, codePoint, length) && isTokenCodePoint(codePoint)) {
            // Keep the original bytes so Hangul survives unchanged.
            token.append(text, index, length);
        } else {
            // Disallowed or malformed code points separate tokens.
            flushToken(tokens, token);
        }
        index += length;
    }

    flushToken(tokens, token);
    return tokens;
}

}  // namespace mini_lexical_search
