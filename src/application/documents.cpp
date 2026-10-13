#include "mini_lexical_search/application/documents.hpp"
#include "mini_lexical_search/infrastructure/document_io.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace mini_lexical_search {

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
        std::uint64_t count = 0;
        readDocumentLines(document, [&](std::uint32_t lineNumber, const std::string& line) {
            for (const auto& token : tokenizer.tokenize(line)) {
                ++count;
                if (sink) {
                    sink(document.id, lineNumber, token);
                }
            }
        });
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
