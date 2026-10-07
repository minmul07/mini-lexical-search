#include "mini_lexical_search/document.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include <openssl/evp.h>

#include "json_internal.hpp"

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

IndexWork prepareIndex(const std::filesystem::path& directory,
                       const std::vector<Document>& previous) {
    IndexWork work;
    work.documents = collectDocuments(directory);
    std::unordered_map<std::string, const Document*> byPath;
    std::uint64_t nextId = 0;
    for (const auto& document : previous) {
        byPath.emplace(document.path, &document);
        nextId = std::max(nextId, static_cast<std::uint64_t>(document.id) + 1);
    }
    for (std::size_t index = 0; index < work.documents.size(); ++index) {
        auto& document = work.documents[index];
        const auto old = byPath.find(document.path);
        if (old == byPath.end()) {
            if (nextId > std::numeric_limits<DocumentId>::max()) {
                throw std::runtime_error("document ID range exhausted");
            }
            document.id = static_cast<DocumentId>(nextId++);
            work.toTokenize.push_back(index);
            ++work.stats.added;
        } else {
            document.id = old->second->id;
            // Tokenization follows the same trigger as metadata indexing: only a
            // changed hash (or title) invalidates and rebuilds the tokens.
            if (document.hash == old->second->hash &&
                document.title == old->second->title) {
                document = *old->second;
                ++work.stats.notUpdated;
            } else {
                work.toTokenize.push_back(index);
                work.removed.push_back(document.id);
                ++work.stats.updated;
            }
            byPath.erase(old);
        }
    }
    for (const auto& entry : byPath) {
        work.removed.push_back(entry.second->id);
    }
    std::sort(work.removed.begin(), work.removed.end());
    work.stats.deleted = byPath.size();
    work.changed = work.stats.added || work.stats.updated || work.stats.deleted;
    return work;
}

IndexWork prepareIndex(const std::filesystem::path& directory,
                       const std::filesystem::path& output) {
    const bool exists = std::filesystem::exists(output);
    auto work = prepareIndex(directory, exists ? loadDocuments(output) : std::vector<Document>{});
    work.changed = !exists || work.changed;
    return work;
}

TokenizeStats tokenizeDocuments(std::vector<Document>& documents,
                                const std::vector<std::size_t>& selection,
                                Tokenizer& tokenizer,
                                const TokenSink& sink) {
    TokenizeStats stats;
    for (const std::size_t index : selection) {
        Document& document = documents.at(index);
        std::ifstream input(document.path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("cannot read document: " + document.path);
        }
        std::uint64_t count = 0;
        std::string line;
        for (std::uint32_t lineNumber = 1; std::getline(input, line); ++lineNumber) {
            for (const auto& token : tokenizer.tokenize(line)) {
                ++count;
                if (sink) {
                    sink(document.id, lineNumber, token);
                }
            }
        }
        if (input.bad()) {
            throw std::runtime_error("cannot read document: " + document.path);
        }
        if (count > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("too many tokens: " + document.path);
        }
        document.tokenCount = static_cast<std::uint32_t>(count);
        ++stats.documents;
        stats.tokens += count;
    }
    return stats;
}

IndexStats indexDocuments(const std::filesystem::path& directory,
                          const std::filesystem::path& output) {
    SimpleTokenizer tokenizer;
    auto work = prepareIndex(directory, output);
    tokenizeDocuments(work.documents, work.toTokenize, tokenizer, {});
    if (work.changed) {
        saveDocuments(work.documents, output);
    }
    return work.stats;
}

}  // namespace mini_lexical_search
