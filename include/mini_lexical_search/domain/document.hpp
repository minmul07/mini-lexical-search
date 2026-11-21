#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace mini_lexical_search {

using DocumentId = std::uint32_t;

struct DocumentFileState {
    std::uint64_t size;
    std::int64_t modifiedSeconds;
    std::uint32_t modifiedNanoseconds;

    bool operator==(const DocumentFileState&) const = default;
};

struct Document {
    DocumentId id;
    std::string path;
    std::string title;
    std::uint32_t tokenCount = 0;
    std::string hash;
    std::optional<DocumentFileState> fileState{};
};

}  // namespace mini_lexical_search
