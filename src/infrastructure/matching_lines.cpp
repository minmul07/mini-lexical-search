#include "mini_lexical_search/infrastructure/matching_lines.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace mini_lexical_search {

std::string readMatchingLineRange(
    const Document& document, MatchingReadPosition& position,
    const std::function<std::uint32_t()>& nextNumber, std::size_t limit,
    const std::function<void(const MatchingLine&)>& sink) {
    std::ifstream input(document.path, std::ios::binary);
    if (!input) return "cannot open " + document.path;
    input.seekg(position.offset);
    if (!input) return "cannot seek " + document.path;
    for (std::size_t emitted = 0; emitted < limit; ++emitted) {
        const auto target = nextNumber();
        if (target == 0) return {};
        if (target <= position.line) throw std::invalid_argument("matching line cursor must advance");
        std::string text;
        while (position.line < target) {
            if (!std::getline(input, text)) {
                return (input.bad() ? "cannot read " : "indexed lines are missing; run /index again: ") + document.path;
            }
            position.offset += static_cast<std::streamoff>(text.size()) + (input.eof() ? 0 : 1);
            ++position.line;
        }
        if (!text.empty() && text.back() == '\r') text.pop_back();
        sink({target, std::move(text)});
    }
    return {};
}

std::vector<MatchingLine> readMatchingLines(const Document& document,
                                          const std::vector<std::uint32_t>& matchedLines,
                                          std::size_t maxMatchingLines) {
    if (matchedLines.empty()) return {};
    if (matchedLines.front() == 0 ||
        std::adjacent_find(matchedLines.begin(), matchedLines.end(),
                           [](auto a, auto b) { return a >= b; }) != matchedLines.end()) {
        throw std::invalid_argument("matched lines must be positive, unique and ascending");
    }
    const auto count = maxMatchingLines == 0 ? matchedLines.size()
                                           : std::min(maxMatchingLines, matchedLines.size());
    std::ifstream file(document.path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open " + document.path);
    std::vector<MatchingLine> result;
    std::string text;
    for (std::uint64_t number = 1; result.size() < count && std::getline(file, text); ++number) {
        if (number == matchedLines[result.size()]) {
            if (!text.empty() && text.back() == '\r') text.pop_back();
            result.push_back({static_cast<std::uint32_t>(number), text});
        }
    }
    if (file.bad()) throw std::runtime_error("cannot read " + document.path);
    if (result.size() != count) {
        throw std::runtime_error("indexed lines are missing; run /index again: " + document.path);
    }
    return result;
}

}  // namespace mini_lexical_search
