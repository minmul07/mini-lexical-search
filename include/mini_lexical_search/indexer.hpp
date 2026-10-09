#pragma once

#include <filesystem>
#include <vector>

#include "mini_lexical_search/document.hpp"
#include "mini_lexical_search/inverted_index.hpp"
#include "mini_lexical_search/tokenizer.hpp"

namespace mini_lexical_search {

// In-memory state of the persisted search index: the document table plus the
// line-aware inverted index. The CLI keeps one snapshot loaded while running.
struct IndexSnapshot {
    std::vector<Document> documents;
    InvertedIndex index;
};

struct IndexUpdate {
    IndexStats metadata;
    TokenizeStats tokens;
};

// Rebuilds the postings of the work's added/updated documents and removes the
// obsolete ones. Tokens are collected into a temporary index first, so `index`
// is left unchanged when reading a document fails.
TokenizeStats applyIndexWork(IndexWork& work,
                             InvertedIndex& index,
                             Tokenizer& tokenizer);

// Saves the new document table and index, then replaces `snapshot`. When the
// save fails the in-memory snapshot is restored from the last saved state (or
// emptied when none exists) before the error is rethrown.
void commitIndexSnapshot(std::vector<Document> documents,
                         IndexSnapshot& snapshot,
                         const std::filesystem::path& output);

// Reads a snapshot file. A missing file yields an empty snapshot; damaged data
// is rejected instead of being overwritten.
IndexSnapshot loadIndexSnapshot(const std::filesystem::path& input);

// Atomically writes a snapshot; the previous file survives a failed save.
void saveIndexSnapshot(const std::vector<Document>& documents,
                       const InvertedIndex& index,
                       const std::filesystem::path& output);

// Full pipeline: compare, tokenize, rebuild postings, and save when the
// document table changed. Returns the counters shown by `/index`.
IndexUpdate updateIndexSnapshot(const std::filesystem::path& directory,
                                IndexSnapshot& snapshot,
                                const std::filesystem::path& output,
                                Tokenizer& tokenizer);

}  // namespace mini_lexical_search
