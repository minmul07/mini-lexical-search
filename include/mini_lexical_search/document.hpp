#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mini_lexical_search {

using DocumentId = std::uint32_t;

struct Document {
    DocumentId id;
    std::string path;
    std::string title;
    std::uint32_t tokenCount = 0;
};

// Recursively collects .txt/.md files in path order. No tokenization is performed.
std::vector<Document> collectDocuments(const std::filesystem::path& directory);

// Replaces the document table only after writing a complete JSON file.
void saveDocuments(const std::vector<Document>& documents,
                   const std::filesystem::path& output);

}  // namespace mini_lexical_search
