#pragma once

#include <cstddef>
#include "mini_lexical_search/document.hpp"

namespace mini_lexical_search {

struct MatchingLine {
    std::uint32_t number;
    std::string text;
};

// Reads the file once up to the last requested line. Input must be positive,
// unique and ascending. Zero limit means all lines; otherwise the first N.
// Throws on read failure or missing requested lines; never updates the index.
std::vector<MatchingLine> readMatchingLines(
    const Document& document,
    const std::vector<std::uint32_t>& matchedLines,
    std::size_t maxMatchingLines = 0);

}  // namespace mini_lexical_search
