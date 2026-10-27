#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "mini_lexical_search/domain/tokenizer.hpp"

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requireTokens(mini_lexical_search::SimpleTokenizer& tokenizer,
                   const std::string& text,
                   const std::vector<std::string>& expected,
                   const std::string& message) {
    const auto actual = tokenizer.tokenize(text);
    if (actual != expected) {
        std::string detail = message + ": [";
        for (const auto& token : actual) {
            detail += "\"" + token + "\" ";
        }
        detail += "]";
        throw std::runtime_error(detail);
    }
}

int main() {
    try {
        mini_lexical_search::SimpleTokenizer tokenizer;

        requireTokens(tokenizer, "", {}, "empty line must produce no tokens");
        requireTokens(tokenizer, "   \t\r\n ", {}, "whitespace must produce no tokens");
        requireTokens(tokenizer, "!!! ... ???", {}, "punctuation-only line must produce no tokens");

        requireTokens(tokenizer, "자료구조를 공부하고", {"자료구조를", "공부하고"},
                      "Korean tokens must be preserved");
        requireTokens(tokenizer, "자료구조가", {"자료구조가"},
                      "Korean particles must survive the P0 tokenizer");
        requireTokens(tokenizer, "있습니다.", {"있습니다"},
                      "trailing punctuation must be dropped");
        requireTokens(tokenizer, "C++에서 자료구조를 배운다", {"c", "에서", "자료구조를", "배운다"},
                      "ASCII punctuation must split tokens");

        requireTokens(tokenizer, "Hello, World!", {"hello", "world"},
                      "ASCII letters must be lowercased");
        requireTokens(tokenizer, "ABC123", {"abc123"}, "letters and digits must be kept");
        requireTokens(tokenizer, "foo-bar_baz", {"foo", "bar", "baz"},
                      "internal punctuation must split tokens");

        requireTokens(tokenizer, "a\tb\rc", {"a", "b", "c"},
                      "tab and carriage return must separate tokens");
        requireTokens(tokenizer, "x x x", {"x", "x", "x"},
                      "repeated tokens must be kept for term frequency");

        requireTokens(tokenizer, "👍좋아요", {"좋아요"},
                      "leading emoji must be treated as a separator");
        requireTokens(tokenizer, "🦊", {}, "emoji-only input must produce no tokens");
        requireTokens(tokenizer, "좋아👍요", {"좋아", "요"},
                      "internal emoji must split tokens");
        requireTokens(tokenizer, "Hello 👍 World", {"hello", "world"},
                      "standalone emoji must be ignored");
        requireTokens(tokenizer, "👨‍👩‍👧가족", {"가족"},
                      "emoji ZWJ sequences must be ignored");

        requireTokens(tokenizer, "시간：10초", {"시간", "10초"},
                      "full-width punctuation must split tokens");
        requireTokens(tokenizer, "ＡＢＣ１２３", {},
                      "full-width alphanumerics must produce no tokens");
        requireTokens(tokenizer, "\xEF\xBB\xBF" "좋아요", {"좋아요"},
                      "leading BOM must be excluded from tokens");
        requireTokens(tokenizer, "앞日本語뒤", {"앞", "뒤"},
                      "other scripts must be excluded from tokens");
        requireTokens(tokenizer, "ㄱㄴㄷ", {"ㄱㄴㄷ"},
                      "Hangul compatibility jamo must be kept");
        requireTokens(tokenizer, "한글ABC123", {"한글abc123"},
                      "adjacent Hangul and ASCII must stay in one token");

        requireTokens(tokenizer, "a\xFF" "b", {"a", "b"},
                      "malformed UTF-8 bytes must split tokens");
        requireTokens(tokenizer, "\xC0\xAF" "x", {"x"},
                      "overlong UTF-8 must split tokens");
        requireTokens(tokenizer, "\xED\xA0\x80" "x", {"x"},
                      "UTF-8 surrogate encodings must split tokens");

        std::cout << "tokenizer_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
