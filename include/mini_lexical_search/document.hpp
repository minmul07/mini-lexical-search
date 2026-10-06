#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "mini_lexical_search/tokenizer.hpp"

namespace mini_lexical_search {

using DocumentId = std::uint32_t;

struct Document {
    DocumentId id;
    std::string path;
    std::string title;
    std::uint32_t tokenCount = 0;
    std::string hash;
};

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

// Recursively collects .txt/.md files and hashes their bytes without tokenizing.
std::vector<Document> collectDocuments(const std::filesystem::path& directory);

// Reads the document table, including legacy entries without a hash.
std::vector<Document> loadDocuments(const std::filesystem::path& input);

// Compares the selected directory with the stored table and classifies each
// document without writing to disk or tokenizing.
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

// Replaces the document table only after writing a complete JSON file.
void saveDocuments(const std::vector<Document>& documents,
                   const std::filesystem::path& output);

}  // namespace mini_lexical_search
