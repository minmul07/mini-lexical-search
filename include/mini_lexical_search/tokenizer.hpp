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

// P0 tokenizer: splits on ASCII whitespace, removes ASCII punctuation and
// control characters, and lowercases ASCII letters. Non-ASCII bytes (for
// example UTF-8 Korean) are preserved as they are.
class SimpleTokenizer final : public Tokenizer {
public:
    std::vector<std::string> tokenize(const std::string& text) override;
};

}  // namespace mini_lexical_search
