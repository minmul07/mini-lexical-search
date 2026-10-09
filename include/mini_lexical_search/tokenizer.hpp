#pragma once

#include <string>
#include <vector>

namespace mini_lexical_search {

// Splits one line of text into searchable tokens. The line number is attached
// by the caller that reads the file, so the tokenizer never sees it.
class Tokenizer {
public:
    virtual ~Tokenizer() = default;

    virtual std::vector<std::string> tokenize(const std::string& text) = 0;
};

// P0 tokenizer: keeps runs of ASCII digits and letters (lowercasing the
// letters) and Hangul code points (syllables and Jamo) as tokens. Every other
// code point -- whitespace, punctuation, emoji, BOM, and other scripts --
// separates tokens and is excluded. Malformed UTF-8 bytes are separators too.
class SimpleTokenizer final : public Tokenizer {
public:
    std::vector<std::string> tokenize(const std::string& text) override;
};

}  // namespace mini_lexical_search
