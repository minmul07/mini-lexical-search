#include "mini_lexical_search/infrastructure/document_io.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "json_internal.hpp"
#include "mini_lexical_search/infrastructure/sha256.hpp"

namespace mini_lexical_search {
namespace {

std::string hashFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot read document: " + path.string());
    }
    Sha256 hasher;
    std::array<char, 65536> buffer;
    while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0) {
        hasher.update(buffer.data(), static_cast<std::size_t>(input.gcount()));
    }
    if (input.bad() || !input.eof()) {
        throw std::runtime_error("cannot read document: " + path.string());
    }
    return toHex(hasher.finish());
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
                             path.generic_string(), path.filename().string(), 0, hashFile(path)});
    }
    return documents;
}

void saveDocuments(const std::vector<Document>& documents,
                   const std::filesystem::path& output) {
    json::writeFileAtomically(output, [&documents](std::ostream& stream) {
        stream << "[\n";
        for (std::size_t i = 0; i < documents.size(); ++i) {
            stream << "  ";
            json::writeDocument(stream, documents[i]);
            stream << (i + 1 < documents.size() ? ",\n" : "\n");
        }
        stream << "]\n";
    });
}

void readDocumentLines(const Document& document, const DocumentLineSink& sink) {
    std::ifstream input(document.path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot read document: " + document.path);
    }
    std::string line;
    for (std::uint32_t lineNumber = 1; std::getline(input, line); ++lineNumber) {
        sink(lineNumber, line);
    }
    if (input.bad()) {
        throw std::runtime_error("cannot read document: " + document.path);
    }
}

}  // namespace mini_lexical_search
