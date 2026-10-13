#pragma once

#include <filesystem>
#include "mini_lexical_search/domain/index_snapshot.hpp"

namespace mini_lexical_search {

// Reads a snapshot file. A missing file yields an empty snapshot; damaged data
// is rejected instead of being overwritten.
IndexSnapshot loadIndexSnapshot(const std::filesystem::path& input);

// Atomically writes a snapshot; the previous file survives a failed save.
void saveIndexSnapshot(const std::vector<Document>& documents,
                       const InvertedIndex& index,
                       const std::filesystem::path& output);

}  // namespace mini_lexical_search
