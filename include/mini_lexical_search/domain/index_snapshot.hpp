#pragma once

#include <vector>
#include "mini_lexical_search/domain/document.hpp"
#include "mini_lexical_search/domain/inverted_index.hpp"

namespace mini_lexical_search {

// In-memory state of the persisted search index: the document table plus the
// line-aware inverted index. The application owns one snapshot while running.
struct IndexSnapshot {
    std::vector<Document> documents;
    InvertedIndex index;
};

}  // namespace mini_lexical_search
