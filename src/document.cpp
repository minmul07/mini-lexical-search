#include "mini_lexical_search/document.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace mini_lexical_search {
namespace {

void writeJsonString(std::ostream& output, const std::string& value) {
    constexpr char hex[] = "0123456789abcdef";
    output << '"';
    for (unsigned char character : value) {
        if (character == '"' || character == '\\') {
            output << '\\' << static_cast<char>(character);
        } else if (character < 0x20) {
            output << "\\u00" << hex[character >> 4] << hex[character & 0xf];
        } else {
            output << static_cast<char>(character);
        }
    }
    output << '"';
}

}  // namespace

std::vector<Document> collectDocuments(const std::filesystem::path& directory) {
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error("not a directory: " + directory.string());
    }

    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_symlink() || !entry.is_regular_file()) {
            continue;
        }
        const auto extension = entry.path().extension();
        if (extension == ".txt" || extension == ".md") {
            paths.push_back(std::filesystem::absolute(entry.path()).lexically_normal());
        }
    }
    std::sort(paths.begin(), paths.end());

    std::vector<Document> documents;
    for (const auto& path : paths) {
        if (documents.size() > std::numeric_limits<DocumentId>::max()) {
            throw std::runtime_error("too many documents");
        }
        documents.push_back({static_cast<DocumentId>(documents.size()),
                             path.generic_string(), path.filename().string(), 0});
    }
    return documents;
}

void saveDocuments(const std::vector<Document>& documents,
                   const std::filesystem::path& output) {
    if (!output.parent_path().empty()) {
        std::filesystem::create_directories(output.parent_path());
    }
    auto temporary = output;
    temporary += ".tmp";
    // Reserve the staging directory so concurrent writers cannot share a file.
    if (!std::filesystem::create_directory(temporary)) {
        throw std::runtime_error("temporary directory already exists: " + temporary.string());
    }
    const auto stagedFile = temporary / "documents.json";
    try {
        std::ofstream stream;
        stream.exceptions(std::ios::failbit | std::ios::badbit);
        stream.open(stagedFile, std::ios::binary | std::ios::trunc);
        stream << "[\n";
        for (std::size_t i = 0; i < documents.size(); ++i) {
            const auto& document = documents[i];
            stream << "  {\"id\": " << document.id << ", \"path\": ";
            writeJsonString(stream, document.path);
            stream << ", \"title\": ";
            writeJsonString(stream, document.title);
            stream << ", \"tokenCount\": " << document.tokenCount << "}";
            stream << (i + 1 < documents.size() ? ",\n" : "\n");
        }
        stream << "]\n";
        stream.close();
        std::filesystem::rename(stagedFile, output);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(stagedFile, ignored);
        std::filesystem::remove(temporary, ignored);
        throw;
    }
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
}

}  // namespace mini_lexical_search
