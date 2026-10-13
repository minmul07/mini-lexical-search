#include "mini_lexical_search/application/search_application.hpp"

#include <chrono>
#include <exception>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "mini_lexical_search/domain/query_processor.hpp"
#include "mini_lexical_search/infrastructure/index_storage.hpp"
#include "mini_lexical_search/infrastructure/matching_lines.hpp"

namespace mini_lexical_search {
namespace {

using Clock = std::chrono::steady_clock;

std::int64_t elapsedMilliseconds(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

}  // namespace

SearchApplication::SearchApplication(std::filesystem::path snapshotPath)
    : snapshotPath_(std::move(snapshotPath)) {
    try {
        snapshot_ = loadIndexSnapshot(snapshotPath_);
    } catch (const std::exception& error) {
        ready_ = false;
        loadError_ = error.what();
    }
}

bool SearchApplication::ready() const { return ready_; }
const std::string& SearchApplication::loadError() const { return loadError_; }
const std::filesystem::path& SearchApplication::snapshotPath() const { return snapshotPath_; }

void SearchApplication::requireReady() const {
    if (!ready_) {
        throw std::runtime_error("index snapshot is not loaded; fix or remove " + snapshotPath_.string());
    }
}

IndexUpdate SearchApplication::index(const std::filesystem::path& directory,
                                      const IndexProgressCallback& progress) {
    requireReady();
    const auto notify = [&](const IndexProgress& event) {
        if (progress) progress(event);
    };
    notify({IndexStage::MetadataStarted, 0, {}, {}});
    const auto metadataStarted = Clock::now();
    auto work = prepareIndex(directory, snapshot_.documents);
    notify({IndexStage::MetadataCompleted, elapsedMilliseconds(metadataStarted), work.stats, {}});

    notify({IndexStage::TokenizationStarted, 0, {}, {}});
    const auto tokenizeStarted = Clock::now();
    const auto tokens = applyIndexWork(work, snapshot_.index, tokenizer_);
    const auto tokenizeElapsed = work.toTokenize.empty() ? 0 : elapsedMilliseconds(tokenizeStarted);
    try {
        notify({IndexStage::TokenizationCompleted, tokenizeElapsed, {}, tokens});
    } catch (...) {
        // The callback can fail after postings were rebuilt but before saving.
        // Recover just as commitIndexSnapshot does on a failed save.
        try {
            snapshot_ = loadIndexSnapshot(snapshotPath_);
        } catch (...) {
            snapshot_ = IndexSnapshot{};
        }
        throw;
    }
    if (work.changed) {
        commitIndexSnapshot(std::move(work.documents), snapshot_, snapshotPath_);
    }
    return {work.stats, tokens};
}

std::vector<SearchResult> SearchApplication::search(const std::string& text,
                                                    std::size_t maxMatchingLines) {
    requireReady();
    const auto query = parseQuery(text, tokenizer_);
    const auto candidates = QueryProcessor{}.process(query, snapshot_.index);
    std::unordered_map<DocumentId, const Document*> documents;
    for (const auto& document : snapshot_.documents) documents.emplace(document.id, &document);

    std::vector<SearchResult> results;
    results.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        SearchResult result;
        result.document = *documents.at(candidate.documentId);
        result.score = candidate.score;
        result.totalMatchingLines = candidate.matchedLines.size();
        try {
            result.lines = readMatchingLines(result.document, candidate.matchedLines, maxMatchingLines);
        } catch (const std::exception& error) {
            result.warning = error.what();
        }
        results.push_back(std::move(result));
    }
    return results;
}

}  // namespace mini_lexical_search
