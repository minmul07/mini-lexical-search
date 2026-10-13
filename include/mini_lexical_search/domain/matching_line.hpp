#pragma once

#include <cstdint>
#include <string>

namespace mini_lexical_search {

struct MatchingLine {
    std::uint32_t number;
    std::string text;
};

}  // namespace mini_lexical_search
