#include "mini_lexical_search/indexer.hpp"

#include <stdexcept>
#include <utility>

namespace mini_lexical_search {

TokenizeStats applyIndexWork(IndexWork& work,
                             InvertedIndex& index,
                             Tokenizer& tokenizer) {
    InvertedIndex pending;
    const TokenSink sink = [&pending](DocumentId documentId,
                                      std::uint32_t line,
                                      const std::string& token) {
        pending.add(token, documentId, line);
    };
    const auto stats = tokenizeDocuments(work.documents, work.toTokenize, tokenizer, sink);
    for (const DocumentId documentId : work.removed) {
        index.removeDocument(documentId);
    }
    index.merge(pending);
    return stats;
}

void commitIndexSnapshot(std::vector<Document> documents,
                         IndexSnapshot& snapshot,
                         const std::filesystem::path& output) {
    try {
        saveIndexSnapshot(documents, snapshot.index, output);
    } catch (...) {
        // The postings were already rebuilt in memory; restore the last
        // persisted state so the next `/index` retries from a consistent point.
        try {
            snapshot = loadIndexSnapshot(output);
        } catch (...) {
            snapshot = IndexSnapshot{};
        }
        throw;
    }
    snapshot.documents = std::move(documents);
}

IndexUpdate updateIndexSnapshot(const std::filesystem::path& directory,
                                IndexSnapshot& snapshot,
                                const std::filesystem::path& output,
                                Tokenizer& tokenizer) {
    auto work = prepareIndex(directory, snapshot.documents);
    IndexUpdate update;
    update.metadata = work.stats;
    update.tokens = applyIndexWork(work, snapshot.index, tokenizer);
    if (work.changed) {
        commitIndexSnapshot(std::move(work.documents), snapshot, output);
    }
    return update;
}

}  // namespace mini_lexical_search
