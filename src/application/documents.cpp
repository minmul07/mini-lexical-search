#include "mini_lexical_search/application/documents.hpp"
#include "mini_lexical_search/infrastructure/document_io.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace mini_lexical_search {
namespace {

enum class HashPolicy { UseFileState, AlwaysHash };

IndexWork prepareIndex(const std::filesystem::path& directory,
                       const std::vector<Document>& previous,
                       const DocumentProgress& progress,
                       HashPolicy policy) {
    IndexWork work;
    const auto paths = collectDocumentPaths(directory);
    work.documents.reserve(paths.size());
    std::unordered_map<std::string, const Document*> byPath;
    byPath.reserve(previous.size());
    std::uint64_t nextId = 0;
    for (const auto& document : previous) {
        byPath.emplace(document.path, &document);
        nextId = std::max(nextId, static_cast<std::uint64_t>(document.id) + 1);
    }
    const bool useFileState = policy == HashPolicy::UseFileState;
    if (progress) progress(0, paths.size());
    for (const auto& path : paths) {
        const auto index = work.documents.size();
        Document document{0, path.generic_string(), path.filename().string(), 0, {}};
        const auto old = byPath.find(document.path);
        if (useFileState) document.fileState = readDocumentFileState(path);
        const bool canReuseHash = useFileState && old != byPath.end() &&
                                  !old->second->hash.empty() &&
                                  document.fileState == old->second->fileState;
        if (canReuseHash) {
            document.hash = old->second->hash;
        } else {
            document.hash = hashDocumentFile(path);
            if (document.fileState) verifyDocumentFileState(path, *document.fileState);
        }
        if (old == byPath.end()) {
            if (nextId > std::numeric_limits<DocumentId>::max()) {
                throw std::runtime_error("document ID range exhausted");
            }
            document.id = static_cast<DocumentId>(nextId++);
            work.toTokenize.push_back(index);
            ++work.stats.added;
        } else {
            document.id = old->second->id;
            if (document.hash == old->second->hash &&
                (useFileState || document.title == old->second->title)) {
                const auto fileState = document.fileState;
                document = *old->second;
                if (useFileState && fileState != document.fileState) {
                    document.fileState = fileState;
                    work.changed = true;
                }
                ++work.stats.notUpdated;
            } else {
                work.toTokenize.push_back(index);
                work.removed.push_back(document.id);
                ++work.stats.updated;
            }
            byPath.erase(old);
        }
        work.documents.push_back(std::move(document));
        if (progress) progress(work.documents.size(), paths.size());
    }
    for (const auto& entry : byPath) {
        work.removed.push_back(entry.second->id);
    }
    std::sort(work.removed.begin(), work.removed.end());
    work.stats.deleted = byPath.size();
    work.changed = work.changed || work.stats.added || work.stats.updated || work.stats.deleted;
    return work;
}

}  // namespace

IndexWork prepareIndex(const std::filesystem::path& directory,
                       const std::vector<Document>& previous,
                       const DocumentProgress& progress) {
    return prepareIndex(directory, previous, progress, HashPolicy::UseFileState);
}

IndexWork prepareIndex(const std::filesystem::path& directory,
                       const std::filesystem::path& output,
                       const DocumentProgress& progress) {
    const bool exists = std::filesystem::exists(output);
    auto work = prepareIndex(directory, exists ? loadDocuments(output) : std::vector<Document>{},
                             progress, HashPolicy::AlwaysHash);
    work.changed = !exists || work.changed;
    return work;
}

TokenizeStats tokenizeDocuments(std::vector<Document>& documents,
                                const std::vector<std::size_t>& selection,
                                Tokenizer& tokenizer,
                                const TokenSink& sink,
                                const DocumentProgress& progress) {
    TokenizeStats stats;
    if (progress) progress(0, selection.size());
    for (const std::size_t index : selection) {
        Document& document = documents.at(index);
        if (document.fileState) verifyDocumentFileState(document.path, *document.fileState);
        std::uint64_t count = 0;
        readDocumentLines(document, [&](std::uint32_t lineNumber, const std::string& line) {
            for (const auto& token : tokenizer.tokenize(line)) {
                ++count;
                if (sink) {
                    sink(document.id, lineNumber, token);
                }
            }
        });
        if (document.fileState) verifyDocumentFileState(document.path, *document.fileState);
        if (count > std::numeric_limits<std::uint32_t>::max()) {
            throw std::runtime_error("too many tokens: " + document.path);
        }
        document.tokenCount = static_cast<std::uint32_t>(count);
        ++stats.documents;
        stats.tokens += count;
        if (progress) progress(stats.documents, selection.size());
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
