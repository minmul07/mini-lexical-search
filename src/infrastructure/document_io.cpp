#include "mini_lexical_search/infrastructure/document_io.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

#include <sys/stat.h>

#include "json_internal.hpp"
#include "mini_lexical_search/infrastructure/sha256.hpp"

namespace mini_lexical_search {

std::string hashDocumentFile(const std::filesystem::path& path) {
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

DocumentFileState readDocumentFileState(const std::filesystem::path& path) {
    struct stat info{};
    if (::lstat(path.c_str(), &info) == -1) {
        const std::error_code error(errno, std::generic_category());
        throw std::filesystem::filesystem_error("cannot inspect document", path, error);
    }
    if (!S_ISREG(info.st_mode) || info.st_size < 0) {
        throw std::runtime_error("not a regular document: " + path.string());
    }
    return {static_cast<std::uint64_t>(info.st_size),
            static_cast<std::int64_t>(info.st_mtim.tv_sec),
            static_cast<std::uint32_t>(info.st_mtim.tv_nsec)};
}

void verifyDocumentFileState(const std::filesystem::path& path, const DocumentFileState& expected) {
    if (readDocumentFileState(path) != expected) {
        throw std::runtime_error("document changed during indexing: " + path.string());
    }
}

std::vector<std::filesystem::path> collectDocumentPaths(const std::filesystem::path& directory) {
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
    return paths;
}

std::vector<Document> collectDocuments(const std::filesystem::path& directory,
                                       const DocumentCollectionProgress& progress) {
    const auto paths = collectDocumentPaths(directory);
    if (progress) progress(0, paths.size());

    std::vector<Document> documents;
    for (const auto& path : paths) {
        if (documents.size() > std::numeric_limits<DocumentId>::max()) {
            throw std::runtime_error("too many documents");
        }
        documents.push_back({static_cast<DocumentId>(documents.size()),
                             path.generic_string(), path.filename().string(), 0, hashDocumentFile(path)});
        if (progress) progress(documents.size(), paths.size());
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
