#include "mini_lexical_search/document.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include <openssl/evp.h>

namespace mini_lexical_search {
namespace {

std::string hashFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot read document: " + path.string());
    }
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(
        EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("cannot initialize SHA-256");
    }
    std::array<char, 65536> buffer;
    while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0) {
        if (EVP_DigestUpdate(context.get(), buffer.data(), input.gcount()) != 1) {
            throw std::runtime_error("cannot hash document: " + path.string());
        }
    }
    if (input.bad() || !input.eof()) {
        throw std::runtime_error("cannot read document: " + path.string());
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest;
    unsigned int length = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &length) != 1) {
        throw std::runtime_error("cannot finalize SHA-256");
    }
    constexpr char hex[] = "0123456789abcdef";
    std::string hash;
    for (unsigned int i = 0; i < length; ++i) {
        hash += hex[digest[i] >> 4];
        hash += hex[digest[i] & 0xf];
    }
    return hash;
}

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
                             path.generic_string(), path.filename().string(), 0, hashFile(path)});
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
            stream << ", \"tokenCount\": " << document.tokenCount << ", \"hash\": ";
            writeJsonString(stream, document.hash);
            stream << "}";
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

IndexStats indexDocuments(const std::filesystem::path& directory,
                          const std::filesystem::path& output) {
    auto documents = collectDocuments(directory);
    const bool exists = std::filesystem::exists(output);
    const auto previous = exists ? loadDocuments(output) : std::vector<Document>{};
    std::unordered_map<std::string, const Document*> byPath;
    std::uint64_t nextId = 0;
    for (const auto& document : previous) {
        byPath.emplace(document.path, &document);
        nextId = std::max(nextId, static_cast<std::uint64_t>(document.id) + 1);
    }
    IndexStats stats;
    for (auto& document : documents) {
        const auto old = byPath.find(document.path);
        if (old == byPath.end()) {
            if (nextId > std::numeric_limits<DocumentId>::max()) {
                throw std::runtime_error("document ID range exhausted");
            }
            document.id = static_cast<DocumentId>(nextId++);
            ++stats.added;
        } else {
            document.id = old->second->id;
            if (document.hash == old->second->hash &&
                document.title == old->second->title && old->second->tokenCount == 0) {
                document = *old->second;
                ++stats.notUpdated;
            } else {
                ++stats.updated;
            }
            byPath.erase(old);
        }
    }
    stats.deleted = byPath.size();
    if (!exists || stats.added || stats.updated || stats.deleted) {
        saveDocuments(documents, output);
    }
    return stats;
}

}  // namespace mini_lexical_search
