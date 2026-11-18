#include "mini_lexical_search/application/indexer.hpp"

#include "mini_lexical_search/infrastructure/index_storage.hpp"

#include <stdexcept>
#include <utility>

namespace mini_lexical_search {

TokenizeStats applyIndexWork(IndexWork& work,
                             InvertedIndex& index,
                             Tokenizer& tokenizer,
                             const DocumentProgress& progress) {
    InvertedIndex pending;
    const TokenSink sink = [&pending](DocumentId documentId,
                                      std::uint32_t line,
                                      const std::string& token) {
        pending.add(token, documentId, line);
    };
    const auto stats = tokenizeDocuments(work.documents, work.toTokenize, tokenizer, sink, progress);
    for (const DocumentId documentId : work.removed) {
        index.removeDocument(documentId);
    }
    index.merge(pending);
    return stats;
}

void commitIndexSnapshot(std::vector<Document> documents,
                         const std::filesystem::path& root,
                         IndexSnapshot& snapshot,
                         const std::filesystem::path& output) {
    try {
        snapshot.root = root;
        snapshot.documents = std::move(documents);
        saveIndexSnapshot(snapshot, output);
    } catch (...) {
        // 메모리의 posting은 이미 재구축되었으므로 마지막 저장 상태로 복구한다.
        // 다음 `/index`는 일관된 상태에서 다시 시도한다.
        try {
            snapshot = loadIndexSnapshot(output);
        } catch (...) {
            snapshot = IndexSnapshot{};
        }
        throw;
    }
}

IndexUpdate updateIndexSnapshot(const std::filesystem::path& directory,
                                IndexSnapshot& snapshot,
                                const std::filesystem::path& output,
                                Tokenizer& tokenizer) {
    auto work = prepareIndex(directory, snapshot.documents);
    IndexUpdate update;
    update.metadata = work.stats;
    update.tokens = applyIndexWork(work, snapshot.index, tokenizer);
    const auto root = std::filesystem::absolute(directory).lexically_normal();
    if (work.changed || root != snapshot.root) {
        commitIndexSnapshot(std::move(work.documents), root, snapshot, output);
    }
    return update;
}

}  // namespace mini_lexical_search
