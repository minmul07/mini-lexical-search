#include "mini_lexical_search/application/search_application.hpp"

#include <chrono>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "mini_lexical_search/application/log.hpp"
#include "mini_lexical_search/application/working_job.hpp"
#include "mini_lexical_search/infrastructure/index_storage.hpp"

namespace mini_lexical_search {
namespace {

using Clock = std::chrono::steady_clock;

std::int64_t elapsedMicroseconds(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
}

void logIndexUpdated(std::int64_t metadataUs, std::int64_t tokenizeUs, const IndexStats& stats) {
    std::ostringstream line;
    line << "index metadata (" << formatElapsedMilliseconds(metadataUs)
         << " ms) tokenization (" << formatElapsedMilliseconds(tokenizeUs) << " ms) "
         << stats.notUpdated << " not updated, " << stats.updated << " updated, " << stats.added
         << " added, " << stats.deleted << " deleted";
    appendLog(line.str());
}

void logIndexFailed(const std::exception& error) {
    appendLog(std::string("index failed: ") + error.what());
}

}  // namespace

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
    clearSearch();
    requireReady();
    try {
        const auto notify = [&](const IndexProgress& event) {
            if (event.stage == IndexStage::MetadataStarted) {
                workingJob().set("updating index...");
            } else if (event.stage == IndexStage::TokenizationStarted) {
                workingJob().set("Tokenizing..");
            }
            if (progress) progress(event);
        };
        notify({IndexStage::MetadataStarted, 0, {}, {}});
        const auto metadataStarted = Clock::now();
        auto work = prepareIndex(directory, snapshot_.documents);
        const auto metadataUs = elapsedMicroseconds(metadataStarted);
        notify({IndexStage::MetadataCompleted, metadataUs, work.stats, {}});

        notify({IndexStage::TokenizationStarted, 0, {}, {}});
        const auto tokenizeStarted = Clock::now();
        const auto tokens = applyIndexWork(work, snapshot_.index, tokenizer_);
        const auto tokenizeUs = work.toTokenize.empty() ? 0 : elapsedMicroseconds(tokenizeStarted);
        try {
            notify({IndexStage::TokenizationCompleted, tokenizeUs, {}, tokens});
        } catch (...) {
            // posting 재구축 후 저장 전에 콜백이 실패할 수 있다.
            // 저장 실패 시 commitIndexSnapshot과 동일하게 상태를 복구한다.
            try {
                snapshot_ = loadIndexSnapshot(snapshotPath_);
            } catch (...) {
                snapshot_ = IndexSnapshot{};
            }
            throw;
        }
        const auto root = std::filesystem::absolute(directory).lexically_normal();
        if (work.changed || root != snapshot_.root) {
            commitIndexSnapshot(std::move(work.documents), root, snapshot_, snapshotPath_);
        }
        logIndexUpdated(metadataUs, tokenizeUs, work.stats);
        workingJob().set("Ready");
        return {work.stats, tokens};
    } catch (const std::exception& error) {
        workingJob().set("Failed to update index");
        logIndexFailed(error);
        throw;
    } catch (...) {
        workingJob().set("Failed to update index");
        appendLog("index failed");
        throw;
    }
}

IndexUpdate SearchApplication::rebuildIndex(const std::filesystem::path& directory,
                                            const IndexProgressCallback& progress) {
    clearSearch();
    IndexSnapshot rebuilt;
    try {
        const auto notify = [&](const IndexProgress& event) {
            if (event.stage == IndexStage::MetadataStarted) {
                workingJob().set("updating index...");
            } else if (event.stage == IndexStage::TokenizationStarted) {
                workingJob().set("Tokenizing..");
            }
            if (progress) progress(event);
        };
        notify({IndexStage::MetadataStarted, 0, {}, {}});
        const auto metadataStarted = Clock::now();
        auto work = prepareIndex(directory, std::vector<Document>{});
        const auto metadataUs = elapsedMicroseconds(metadataStarted);
        notify({IndexStage::MetadataCompleted, metadataUs, work.stats, {}});

        notify({IndexStage::TokenizationStarted, 0, {}, {}});
        const auto tokenizeStarted = Clock::now();
        const auto tokens = applyIndexWork(work, rebuilt.index, tokenizer_);
        const auto tokenizeUs = work.toTokenize.empty() ? 0 : elapsedMicroseconds(tokenizeStarted);
        notify({IndexStage::TokenizationCompleted, tokenizeUs, {}, tokens});
        commitIndexSnapshot(std::move(work.documents), std::filesystem::absolute(directory).lexically_normal(),
                            rebuilt, snapshotPath_);
        logIndexUpdated(metadataUs, tokenizeUs, work.stats);
        snapshot_ = std::move(rebuilt);
        ready_ = true;
        loadError_.clear();
        workingJob().set("Ready");
        return {work.stats, tokens};
    } catch (const std::exception& error) {
        workingJob().set("Failed to load index");
        logIndexFailed(error);
        throw;
    } catch (...) {
        workingJob().set("Failed to load index");
        appendLog("index failed");
        throw;
    }
}

}  // namespace mini_lexical_search
