#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "mini_lexical_search/application/indexer.hpp"
#include "mini_lexical_search/domain/matching_line.hpp"

namespace mini_lexical_search {

enum class IndexStage {
    MetadataStarted,
    MetadataCompleted,
    TokenizationStarted,
    TokenizationCompleted,
};

struct IndexProgress {
    IndexStage stage;
    std::int64_t elapsedMilliseconds = 0;
    IndexStats metadata;
    TokenizeStats tokens;
};

// Called synchronously. Do not reenter or concurrently access the application.
// Exceptions abort indexing; rebuilt postings are restored before propagation.
using IndexProgressCallback = std::function<void(const IndexProgress&)>;

struct SearchResult {
    Document document;
    double score = 0;
    std::size_t totalMatchingLines = 0;
    std::vector<MatchingLine> lines;
    // A document read failure does not discard other results.
    std::string warning;
};

// UI-independent, synchronous search session. Owns the loaded index and
// tokenizer; returns values without exposing mutable snapshot state.
class SearchApplication {
public:
    explicit SearchApplication(std::filesystem::path snapshotPath);

    bool ready() const;
    const std::string& loadError() const;
    const std::filesystem::path& snapshotPath() const;

    // Throws on a failed initial load, indexing, saving, or callback failure.
    // Completion events describe the named stage, not successful persistence.
    IndexUpdate index(const std::filesystem::path& directory,
                      const IndexProgressCallback& progress = {});

    // Returns document-ID-ordered results; 0 means all matching lines.
    // Throws on a failed initial load or invalid query. Never writes snapshots.
    std::vector<SearchResult> search(const std::string& query,
                                     std::size_t maxMatchingLines = 0);

private:
    void requireReady() const;

    std::filesystem::path snapshotPath_;
    IndexSnapshot snapshot_;
    SimpleTokenizer tokenizer_;
    bool ready_ = true;
    std::string loadError_;
};

}  // namespace mini_lexical_search
