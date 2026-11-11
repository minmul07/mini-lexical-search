#include "mini_lexical_search/application/search_application.hpp"
#include "mini_lexical_search/application/working_job.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace mini_lexical_search;

namespace {

std::vector<SearchResult> search(SearchApplication& app, const std::string& query) {
    return app.search(query).files;
}


void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void write(const fs::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary);
    output << text;
    require(static_cast<bool>(output), "fixture write failed");
}

std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "fixture read failed");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

template <typename Action>
void requireFailure(Action action, const std::string& message) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    require(failed, message);
}

bool isMilestone(IndexStage stage) {
    return stage == IndexStage::MetadataStarted || stage == IndexStage::MetadataCompleted ||
           stage == IndexStage::TokenizationStarted || stage == IndexStage::TokenizationCompleted;
}

std::vector<IndexProgress> milestones(const std::vector<IndexProgress>& events) {
    std::vector<IndexProgress> result;
    for (const auto& event : events) {
        if (isMilestone(event.stage)) result.push_back(event);
    }
    return result;
}

const IndexProgress* findStage(const std::vector<IndexProgress>& events, IndexStage stage) {
    for (const auto& event : events) {
        if (event.stage == stage) return &event;
    }
    return nullptr;
}

const IndexProgress* findLastStage(const std::vector<IndexProgress>& events, IndexStage stage) {
    const IndexProgress* result = nullptr;
    for (const auto& event : events) {
        if (event.stage == stage) result = &event;
    }
    return result;
}

void requireStages(const std::vector<IndexProgress>& events) {
    const std::vector<IndexStage> expected{
        IndexStage::MetadataStarted, IndexStage::MetadataCompleted,
        IndexStage::TokenizationStarted, IndexStage::TokenizationCompleted};
    const auto actual = milestones(events);
    require(actual.size() == expected.size(), "wrong milestone event count");
    for (std::size_t i = 0; i < actual.size(); ++i) {
        require(actual[i].stage == expected[i], "wrong event order");
        require(actual[i].elapsedMicroseconds >= 0, "negative elapsed time");
    }
}

void testSession(const fs::path& root) {
    const auto documents = root / "documents";
    const auto snapshot = root / "data/index.bin";
    fs::create_directories(documents);
    write(documents / "a.md", "Alpha beta alpha\nalpha\n자료구조 알고리즘\n");
    write(documents / "b.txt", "beta\nalpha\n자료구조\n");
    SearchApplication app(snapshot);
    require(app.ready() && app.loadError().empty(), "missing snapshot must start ready");
    require(workingJob().label() == "Ready", "missing snapshot must be ready");
    require(app.snapshotPath() == snapshot && search(app, "alpha").empty(), "empty session failed");

    std::vector<IndexProgress> events;
    auto collect = [&](const IndexProgress& event) { events.push_back(event); };
    const auto update = app.index(documents, collect);
    requireStages(events);
    require(workingJob().label() == "Ready", "successful indexing must be ready");
    require(update.metadata.added == 2 && update.tokens.documents == 2 && update.tokens.tokens == 9,
            "wrong indexing counters");
    const auto* metadataCompleted = findStage(events, IndexStage::MetadataCompleted);
    const auto* tokenizationCompleted = findStage(events, IndexStage::TokenizationCompleted);
    require(metadataCompleted && tokenizationCompleted &&
                metadataCompleted->metadata.added == 2 && tokenizationCompleted->tokens.tokens == 9,
            "event counters missing");
    const auto* metadataEnd = findLastStage(events, IndexStage::MetadataProgress);
    require(metadataEnd && metadataEnd->completed == 2 && metadataEnd->total == 2,
            "metadata progress missing");
    const auto* tokenEnd = findLastStage(events, IndexStage::TokenizationProgress);
    require(tokenEnd && tokenEnd->completed == 2 && tokenEnd->total == 2 &&
                findStage(events, IndexStage::SaveStarted) &&
                findStage(events, IndexStage::SaveCompleted),
            "tokenization/save progress missing");
    std::size_t previousMetadata = 0;
    std::size_t previousTokens = 0;
    for (const auto& event : events) {
        if (event.stage == IndexStage::MetadataProgress) {
            require(event.completed >= previousMetadata && event.completed <= event.total,
                    "metadata progress is not monotonic");
            previousMetadata = event.completed;
        }
        if (event.stage == IndexStage::TokenizationProgress) {
            require(event.completed >= previousTokens && event.completed <= event.total,
                    "tokenization progress is not monotonic");
            previousTokens = event.completed;
        }
    }

    const auto results = search(app, "ALPHA");
    require(results.size() == 2 && results[0].document.id < results[1].document.id,
            "result order or normalization failed");
    require(results[0].totalMatchingLines == 2 && results[0].lines.size() == 2,
            "matching lines failed");
    require(results[0].lines[0].number == 1 && results[0].lines[1].number == 2 &&
            results[0].lines[0].text == "Alpha beta alpha" && results[0].warning.empty(),
            "matching line values failed");
    require(results[0].document.tokenCount == 6 && !results[0].document.hash.empty(),
            "document information missing");
    require(search(app, "자료구조 AND 알고리즘").size() == 1, "AND search failed");
    require(search(app, "자료구조 OR 알고리즘").size() == 2, "OR search failed");
    require(search(app, "absent").empty(), "nonmatching query failed");
    requireFailure([&] { search(app, "alpha beta"); }, "invalid query accepted");
    require(search(app, "alpha").size() == 2, "query failure broke session");

    const auto saved = read(snapshot);
    const auto modified = fs::last_write_time(snapshot);
    SearchApplication reopened(snapshot);
    require(reopened.ready() && search(reopened, "alpha")[0].totalMatchingLines == 2, "reload failed");
    events.clear();
    const auto unchanged = reopened.index(documents, collect);
    requireStages(events);
    const auto* unchangedTokenization = findStage(events, IndexStage::TokenizationCompleted);
    require(unchanged.metadata.notUpdated == 2 && unchanged.tokens.documents == 0 &&
                unchangedTokenization && unchangedTokenization->elapsedMicroseconds == 0,
            "unchanged indexing failed");
    require(!findStage(events, IndexStage::SaveStarted), "unchanged indexing saved a snapshot");
    require(read(snapshot) == saved && fs::last_write_time(snapshot) == modified,
            "unchanged indexing rewrote snapshot");

    events.clear();
    requireFailure([&] { reopened.index(root / "missing", collect); }, "missing directory accepted");
    require(events.size() == 1 && events[0].stage == IndexStage::MetadataStarted,
            "failed metadata emitted completion");

    // A file removed after metadata completion must abort tokenization safely.
    const auto transient = documents / "c.txt";
    write(transient, "transient\n");
    events.clear();
    requireFailure([&] {
        reopened.index(documents, [&](const IndexProgress& event) {
            events.push_back(event);
            if (event.stage == IndexStage::MetadataCompleted) fs::remove(transient);
        });
    }, "missing tokenization input accepted");
    const auto failedMilestones = milestones(events);
    require(failedMilestones.size() == 3 &&
                failedMilestones.back().stage == IndexStage::TokenizationStarted &&
                !findStage(events, IndexStage::TokenizationCompleted),
            "failed tokenization emitted completion");
    require(read(snapshot) == saved && search(reopened, "alpha").size() == 2,
            "tokenization failure changed session");

    write(documents / "a.md", "replacement\n");
    fs::create_directory(snapshot.string() + ".tmp");
    events.clear();
    requireFailure([&] { reopened.index(documents, collect); }, "blocked save accepted");
    requireStages(events);
    require(read(snapshot) == saved && search(reopened, "alpha")[0].totalMatchingLines == 2 &&
            search(reopened, "replacement").empty(), "save failure did not restore postings");
    fs::remove(snapshot.string() + ".tmp");

    // A save-stage observer exception must restore postings that were already merged.
    requireFailure([&] {
        reopened.index(documents, [](const IndexProgress& event) {
            if (event.stage == IndexStage::SaveStarted) throw std::runtime_error("save observer");
        });
    }, "save observer exception swallowed");
    require(read(snapshot) == saved && search(reopened, "replacement").empty(),
            "save observer failure changed session");

    // A UI observer exception after rebuilding also restores the saved index.
    requireFailure([&] {
        reopened.index(documents, [](const IndexProgress& event) {
            if (event.stage == IndexStage::TokenizationCompleted) throw std::runtime_error("observer");
        });
    }, "observer exception swallowed");
    require(read(snapshot) == saved && search(reopened, "replacement").empty(),
            "observer failure changed session");
    const auto retry = reopened.index(documents);
    require(retry.metadata.updated == 1 && search(reopened, "replacement").size() == 1,
            "retry after failed save/observer failed");
    require(results[0].document.tokenCount == 6,
            "previous results were invalidated by indexing");

    // Original file loss is a per-document warning, not a search failure.
    fs::remove(documents / "a.md");
    const auto beforeSearch = read(snapshot);
    const auto beforeTime = fs::last_write_time(snapshot);
    const auto warnings = search(reopened, "replacement OR beta");
    require(warnings.size() == 2 && !warnings[0].warning.empty() && warnings[0].lines.empty() &&
            warnings[0].totalMatchingLines == 1 && warnings[1].warning.empty() &&
            warnings[1].lines.size() == 1, "document warning discarded later results");
    require(read(snapshot) == beforeSearch && fs::last_write_time(snapshot) == beforeTime,
            "search changed snapshot");
}

void testCorruptionAndFirstSave(const fs::path& root) {
    fs::create_directories(root);
    const auto corrupt = root / "corrupt.bin";
    write(corrupt, "not a binary index");
    SearchApplication broken(corrupt);
    require(!broken.ready() && !broken.loadError().empty(), "corrupt snapshot accepted");
    require(workingJob().label() == "Failed to load index", "corrupt snapshot must fail the job");
    requireFailure([&] { search(broken, "alpha"); }, "search on corrupt session accepted");
    std::vector<IndexProgress> events;
    requireFailure([&] { broken.index(root, [&](const IndexProgress& e) { events.push_back(e); }); },
                   "index on corrupt session accepted");
    require(events.empty() && read(corrupt) == "not a binary index",
            "corrupt snapshot overwritten");

    const auto documents = root / "documents";
    fs::create_directories(documents);
    write(documents / "a.txt", "alpha\n");
    fs::create_directory(corrupt.string() + ".tmp");
    requireFailure([&] { broken.rebuildIndex(documents); }, "blocked rebuild accepted");
    require(!broken.ready() && workingJob().label() == "Failed to load index",
            "failed rebuild became ready");
    require(read(corrupt) == "not a binary index", "failed rebuild overwrote the snapshot");
    fs::remove(corrupt.string() + ".tmp");
    const auto rebuilt = broken.rebuildIndex(documents);
    require(broken.ready() && workingJob().label() == "Ready" && rebuilt.metadata.added == 1 &&
                search(broken, "alpha").size() == 1,
            "rebuild did not replace the damaged snapshot");
    require(read(corrupt) != "not a binary index", "rebuild kept the damaged text");

    fs::create_directory(documents);
    write(documents / "a.txt", "alpha\n");
    const auto snapshot = root / "new.bin";
    SearchApplication fresh(snapshot);
    fs::create_directory(snapshot.string() + ".tmp");
    requireFailure([&] { fresh.index(documents); }, "first blocked save accepted");
    require(!fs::exists(snapshot) && search(fresh, "alpha").empty(), "first failure retained unsaved state");
    fs::remove(snapshot.string() + ".tmp");
    require(fresh.index(documents).metadata.added == 1 && search(fresh, "alpha").size() == 1,
            "first save retry failed");
}

void testBrowsing(const fs::path& root) {
    const auto docs = root / "documents";
    fs::create_directories(docs / "z-dir/한글 폴더");
    write(docs / "a-root.txt", "hit hit beta\n");
    write(docs / "z-dir/a.txt", "hit\nnoise\nhit beta\n");
    write(docs / "z-dir/한글 폴더/b.md", "hit\n");
    SearchApplication app(root / "index.bin");
    app.index(docs);
    const auto page = app.search("hit OR beta");
    require(page.files.size() == 3 && page.files[0].relativePath == "z-dir/한글 폴더/b.md" &&
            page.files[1].relativePath == "z-dir/a.txt" && page.files[2].relativePath == "a-root.txt",
            "directory-first traversal failed");
    require(page.files[2].totalMatchingLines == 1 && page.files[1].lines[1].number == 3,
            "row union/dedup failed");
    require(app.search("hit AND beta").files.size() == 2, "document-level AND changed");
    require(app.search("hit OR hit").files.size() == 3, "repeated OR changed matches");
    write(docs / "z-dir/0-new.txt", "hit\n");
    fs::rename(docs / "a-root.txt", docs / "b-root.txt");
    app.index(docs);
    requireFailure([&] { app.currentPage(); }, "index did not clear session");
    const auto updated = app.search("hit");
    require(updated.files[1].relativePath == "z-dir/0-new.txt" &&
            updated.files[1].document.id > updated.files[2].document.id,
            "incremental traversal used document IDs");
    SearchApplication reopened(root / "index.bin");
    require(reopened.search("hit").files[0].relativePath == page.files[0].relativePath,
            "reload lost root/order");

    // Streaming returns every row in traversal order without keeping a pageable session.
    std::vector<std::string> paths;
    std::size_t rows = 0;
    reopened.visitAllResults("hit", [&](const SearchResult& group) { paths.push_back(group.relativePath); },
        [&](const MatchingLine&) { ++rows; }, [&](const std::string&) { throw std::runtime_error("unexpected warning"); });
    require(paths.size() == 4 && rows == 5, "stream omitted results");
    requireFailure([&] { reopened.currentPage(); }, "stream retained navigable session");
    requireFailure([&] { reopened.visitAllResults("hit", [](const SearchResult&) {},
        [](const MatchingLine&) { throw std::runtime_error("sink"); }, [](const std::string&) {}); },
        "sink failure was swallowed");
}

std::string hits(int count) {
    std::string text;
    for (int i = 0; i < count; ++i) text += "hit hit beta\r\n";
    return text;
}

void testPages(const fs::path& root) {
    const auto docs = root / "documents";
    fs::create_directories(docs);
    // 10 + 10 + 10 + 10 + 9 = 49, then a whole 10-row group => 59.
    const std::vector<int> counts{100, 11, 10, 10, 9, 10, 1, 0};
    for (std::size_t i = 0; i < counts.size(); ++i) {
        write(docs / (std::to_string(i) + " file.txt"), hits(counts[i]));
    }
    SearchApplication app(root / "index.bin");
    app.index(docs);
    auto first = app.search("hit AND beta");
    require(first.number == 1 && !first.hasPrevious && first.hasNext && first.files.size() == 6,
            "soft page boundary failed");
    std::size_t displayed = 0;
    for (const auto& file : first.files) displayed += file.lines.size();
    require(displayed == 59 && first.files[0].totalMatchingLines == 100 &&
            first.files[1].totalMatchingLines == 11 && first.files[4].totalMatchingLines == 9,
            "initial group sizes/counts failed");
    require(first.files[0].lines[0].text == "hit hit beta", "CRLF not stripped");
    auto expanded = app.expandFile("0 file.txt");
    require(expanded.files[0].lines.size() == 20 && expanded.files.size() == 6, "expansion moved files");
    requireFailure([&] { app.expandFile("6 file.txt"); }, "off-page file expanded");
    auto second = app.nextPage();
    require(second.number == 2 && second.hasPrevious && !second.hasNext && second.files.size() == 1,
            "next page repeated hidden rows or empty files");
    require(app.nextPage().number == 2, "next crossed last page");
    require(app.previousPage().files[0].lines.size() == 20, "previous lost expansion");
    require(app.previousPage().number == 1, "previous crossed first page");
    for (int i = 0; i < 9; ++i) expanded = app.expandFile("0 file.txt");
    require(expanded.files[0].lines.size() == 100 && expanded.files[0].lines.back().number == 100,
            "repeated expansion lost rows");
    for (std::size_t i = 0; i < expanded.files[0].lines.size(); ++i) {
        require(expanded.files[0].lines[i].number == i + 1, "expanded rows duplicated/reordered");
    }
    require(app.expandFile("0 file.txt").files[0].lines.size() == 100, "exhausted expansion changed rows");
    // Already cached data survives original loss; further reads fail once, preserving earlier rows.
    fs::remove(docs / "1 file.txt");
    const auto failed = app.expandFile("1 file.txt");
    require(!failed.files[1].warning.empty() && failed.files[1].lines.size() == 10,
            "failed expansion discarded cached rows");
    write(docs / "1 file.txt", hits(11));
    require(app.expandFile("1 file.txt").files[1].lines.size() == 10, "failed file was retried in session");
    require(app.search("hit").files[1].warning.empty(), "new query did not retry failed file");
    requireFailure([&] { app.search("hit OR"); }, "bad query accepted");
    requireFailure([&] { app.currentPage(); }, "bad query retained old session");
    app.search("hit");
    requireFailure([&] { app.index(root / "missing"); }, "missing directory accepted");
    requireFailure([&] { app.nextPage(); }, "failed indexing retained session");

    std::size_t streamed = 0, groups = 0;
    app.visitAllResults("hit OR beta", [&](const SearchResult&) { ++groups; },
        [&](const MatchingLine&) { ++streamed; }, [](const std::string&) {});
    require(groups == 7 && streamed == 151, "unlimited stream truncated results");
}

void testPageBoundaries(const fs::path& root) {
    for (const int count : {0, 1, 49, 50, 51, 100, 101}) {
        const auto fixture = root / std::to_string(count);
        const auto docs = fixture / "docs";
        fs::create_directories(docs);
        for (int i = 0; i < count; ++i) write(docs / (std::to_string(i) + ".txt"), "hit\n");
        SearchApplication app(fixture / "index.bin");
        app.index(docs);
        auto page = app.search("hit");
        std::size_t total = page.files.size();
        while (page.hasNext) {
            page = app.nextPage();
            require(page.files.size() <= 50, "one-row groups exceeded threshold");
            total += page.files.size();
        }
        require(total == static_cast<std::size_t>(count), "page traversal omitted files");
        require(app.nextPage().number == page.number, "last page changed");
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    try {
        const fs::path root = argv[1];
        fs::remove_all(root);
        fs::create_directories(root);
        testSession(root / "session");
        testBrowsing(root / "browsing");
        testPages(root / "pages");
        testPageBoundaries(root / "boundaries");
        testCorruptionAndFirstSave(root / "failures");
        fs::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
