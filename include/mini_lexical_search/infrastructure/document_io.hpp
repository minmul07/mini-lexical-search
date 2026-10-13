#pragma once

#include <filesystem>
#include <functional>
#include <vector>
#include "mini_lexical_search/domain/document.hpp"

namespace mini_lexical_search {

// Recursively collects .txt/.md files and hashes their bytes without tokenizing.
std::vector<Document> collectDocuments(const std::filesystem::path& directory);

// Reads the document table, including legacy entries without a hash.
std::vector<Document> loadDocuments(const std::filesystem::path& input);

// Replaces the document table only after writing a complete JSON file.
void saveDocuments(const std::vector<Document>& documents,
                   const std::filesystem::path& output);

// Reads a document sequentially, emitting original 1-based line numbers.
// Throws on file open/read failure; callbacks run synchronously.
using DocumentLineSink = std::function<void(std::uint32_t, const std::string&)>;
void readDocumentLines(const Document& document, const DocumentLineSink& sink);

}  // namespace mini_lexical_search
