#pragma once

#include <filesystem>
#include <functional>
#include <vector>
#include "mini_lexical_search/domain/document.hpp"
#include "mini_lexical_search/domain/tokenizer.hpp"

namespace mini_lexical_search {

struct IndexStats {
    std::size_t notUpdated = 0;
    std::size_t updated = 0;
    std::size_t added = 0;
    std::size_t deleted = 0;
};

// Result of comparing the stored document table with a directory, before any
// tokenization. Unchanged documents keep their previous IDs and tokenCount.
struct IndexWork {
    std::vector<Document> documents;
    // Indices into `documents` of added or updated documents that still need
    // tokenization. Empty when every document is unchanged.
    std::vector<std::size_t> toTokenize;
    // IDs whose previous postings must be discarded: updated and deleted
    // documents, ascending. Empty when no inverted index entry is stale.
    std::vector<DocumentId> removed;
    IndexStats stats;
    bool changed = false;  // Whether the stored table must be rewritten.
};

struct TokenizeStats {
    std::size_t documents = 0;
    std::uint64_t tokens = 0;
};

// Receives every token produced during tokenization together with its document
// and original 1-based line number. Empty for metadata-only callers.
using TokenSink = std::function<void(DocumentId, std::uint32_t, const std::string&)>;

// Compares the selected directory with a previously stored document table and
// classifies each document without writing to disk or tokenizing.
IndexWork prepareIndex(const std::filesystem::path& directory,
                       const std::vector<Document>& previous);

// Same comparison after loading the previous table from a JSON file. A missing
// file is treated as an empty table and always marks the work as changed.
IndexWork prepareIndex(const std::filesystem::path& directory,
                       const std::filesystem::path& output);

// Tokenizes the selected documents line by line, filling `tokenCount` and
// forwarding every token to `sink` (when set). Only the given selection is
// read; unchanged documents are skipped by the caller.
TokenizeStats tokenizeDocuments(std::vector<Document>& documents,
                                const std::vector<std::size_t>& selection,
                                Tokenizer& tokenizer,
                                const TokenSink& sink);

// Runs the full pipeline with a SimpleTokenizer: synchronize, tokenize changed
// documents, and save when the table changed.
IndexStats indexDocuments(const std::filesystem::path& directory,
                          const std::filesystem::path& output);

}  // namespace mini_lexical_search
