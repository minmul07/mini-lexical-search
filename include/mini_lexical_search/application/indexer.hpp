#pragma once

#include "mini_lexical_search/application/documents.hpp"
#include "mini_lexical_search/domain/index_snapshot.hpp"

namespace mini_lexical_search {

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

// Full pipeline: compare, tokenize, rebuild postings, and save when the
// document table changed. Returns the counters shown by `/index`.
IndexUpdate updateIndexSnapshot(const std::filesystem::path& directory,
                                IndexSnapshot& snapshot,
                                const std::filesystem::path& output,
                                Tokenizer& tokenizer);

}  // namespace mini_lexical_search
