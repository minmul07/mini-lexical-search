#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "mini_lexical_search/tokenizer.hpp"

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
        requireTokens(tokenizer, "C++에서 자료구조를 배운다", {"c에서", "자료구조를", "배운다"},
                      "ASCII punctuation must be removed");

        requireTokens(tokenizer, "Hello, World!", {"hello", "world"},
                      "ASCII letters must be lowercased");
        requireTokens(tokenizer, "ABC123", {"abc123"}, "letters and digits must be kept");
        requireTokens(tokenizer, "foo-bar_baz", {"foobarbaz"},
                      "internal punctuation must be removed");

        requireTokens(tokenizer, "a\tb\rc", {"a", "b", "c"},
                      "tab and carriage return must separate tokens");
        requireTokens(tokenizer, "x x x", {"x", "x", "x"},
                      "repeated tokens must be kept for term frequency");

        std::cout << "tokenizer_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
