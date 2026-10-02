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
    std::string hash;
};

struct IndexStats {
    std::size_t notUpdated = 0;
    std::size_t updated = 0;
    std::size_t added = 0;
    std::size_t deleted = 0;
};

// Recursively collects .txt/.md files and hashes their bytes without tokenizing.
std::vector<Document> collectDocuments(const std::filesystem::path& directory);

// Reads the document table, including legacy entries without a hash.
std::vector<Document> loadDocuments(const std::filesystem::path& input);

// Synchronizes one document table with the selected directory, preserving IDs.
IndexStats indexDocuments(const std::filesystem::path& directory,
                          const std::filesystem::path& output);

// Replaces the document table only after writing a complete JSON file.
void saveDocuments(const std::vector<Document>& documents,
                   const std::filesystem::path& output);

}  // namespace mini_lexical_search
