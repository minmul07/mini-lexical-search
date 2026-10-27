#pragma once

#include <cstdint>
#include <string>

namespace mini_lexical_search {

using DocumentId = std::uint32_t;

struct Document {
    DocumentId id;
    std::string path;
    std::string title;
    std::uint32_t tokenCount = 0;
    std::string hash;
};

}  // namespace mini_lexical_search
