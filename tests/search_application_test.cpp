#include "mini_lexical_search/application/search_application.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace mini_lexical_search;

namespace {

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

void requireStages(const std::vector<IndexProgress>& events) {
    const std::vector<IndexStage> expected{
        IndexStage::MetadataStarted, IndexStage::MetadataCompleted,
        IndexStage::TokenizationStarted, IndexStage::TokenizationCompleted};
    require(events.size() == expected.size(), "wrong event count");
    for (std::size_t i = 0; i < events.size(); ++i) {
        require(events[i].stage == expected[i], "wrong event order");
        require(events[i].elapsedMilliseconds >= 0, "negative elapsed time");
    }
}

void testSession(const fs::path& root) {
    const auto documents = root / "documents";
    const auto snapshot = root / "data/index.json";
    fs::create_directories(documents);
    write(documents / "a.md", "Alpha beta alpha\nalpha\n자료구조 알고리즘\n");
    write(documents / "b.txt", "beta\nalpha\n자료구조\n");
    SearchApplication app(snapshot);
    require(app.ready() && app.loadError().empty(), "missing snapshot must start ready");
    require(app.snapshotPath() == snapshot && app.search("alpha").empty(), "empty session failed");

    std::vector<IndexProgress> events;
    auto collect = [&](const IndexProgress& event) { events.push_back(event); };
    const auto update = app.index(documents, collect);
    requireStages(events);
    require(update.metadata.added == 2 && update.tokens.documents == 2 && update.tokens.tokens == 9,
            "wrong indexing counters");
    require(events[1].metadata.added == 2 && events[3].tokens.tokens == 9, "event counters missing");

    const auto results = app.search("ALPHA");
    require(results.size() == 2 && results[0].document.id < results[1].document.id,
            "result order or normalization failed");
    require(results[0].score == 3 && results[0].totalMatchingLines == 2 && results[0].lines.size() == 2,
            "scores or matching lines failed");
    require(results[0].lines[0].number == 1 && results[0].lines[1].number == 2 &&
            results[0].lines[0].text == "Alpha beta alpha" && results[0].warning.empty(),
            "matching line values failed");
    require(results[0].document.tokenCount == 6 && !results[0].document.hash.empty(),
            "document information missing");
    const auto limited = app.search("alpha", 1);
    require(limited[0].lines.size() == 1 && limited[0].totalMatchingLines == 2,
            "line limit lost total count");
    require(app.search("자료구조 AND 알고리즘").size() == 1, "AND search failed");
    require(app.search("자료구조 OR 알고리즘").size() == 2, "OR search failed");
    require(app.search("absent").empty(), "nonmatching query failed");
    requireFailure([&] { app.search("alpha beta"); }, "invalid query accepted");
    require(app.search("alpha").size() == 2, "query failure broke session");

    const auto saved = read(snapshot);
    const auto modified = fs::last_write_time(snapshot);
    SearchApplication reopened(snapshot);
    require(reopened.ready() && reopened.search("alpha")[0].score == 3, "reload failed");
    events.clear();
    const auto unchanged = reopened.index(documents, collect);
    requireStages(events);
    require(unchanged.metadata.notUpdated == 2 && unchanged.tokens.documents == 0 &&
            events[3].elapsedMilliseconds == 0, "unchanged indexing failed");
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
    require(events.size() == 3 && events.back().stage == IndexStage::TokenizationStarted,
            "failed tokenization emitted completion");
    require(read(snapshot) == saved && reopened.search("alpha").size() == 2,
            "tokenization failure changed session");

    write(documents / "a.md", "replacement\n");
    fs::create_directory(snapshot.string() + ".tmp");
    events.clear();
    requireFailure([&] { reopened.index(documents, collect); }, "blocked save accepted");
    requireStages(events);
    require(read(snapshot) == saved && reopened.search("alpha")[0].score == 3 &&
            reopened.search("replacement").empty(), "save failure did not restore postings");
    fs::remove(snapshot.string() + ".tmp");

    // A UI observer exception after rebuilding also restores the saved index.
    requireFailure([&] {
        reopened.index(documents, [](const IndexProgress& event) {
            if (event.stage == IndexStage::TokenizationCompleted) throw std::runtime_error("observer");
        });
    }, "observer exception swallowed");
    require(read(snapshot) == saved && reopened.search("replacement").empty(),
            "observer failure changed session");
    const auto retry = reopened.index(documents);
    require(retry.metadata.updated == 1 && reopened.search("replacement").size() == 1,
            "retry after failed save/observer failed");
    require(results[0].score == 3 && results[0].document.tokenCount == 6,
            "previous results were invalidated by indexing");

    // Original file loss is a per-document warning, not a search failure.
    fs::remove(documents / "a.md");
    const auto beforeSearch = read(snapshot);
    const auto beforeTime = fs::last_write_time(snapshot);
    const auto warnings = reopened.search("replacement OR beta");
    require(warnings.size() == 2 && !warnings[0].warning.empty() && warnings[0].lines.empty() &&
            warnings[0].totalMatchingLines == 1 && warnings[1].warning.empty() &&
            warnings[1].lines.size() == 1, "document warning discarded later results");
    require(read(snapshot) == beforeSearch && fs::last_write_time(snapshot) == beforeTime,
            "search changed snapshot");
}

void testCorruptionAndFirstSave(const fs::path& root) {
    fs::create_directories(root);
    const auto corrupt = root / "corrupt.json";
    write(corrupt, "{broken");
    SearchApplication broken(corrupt);
    require(!broken.ready() && !broken.loadError().empty(), "corrupt snapshot accepted");
    requireFailure([&] { broken.search("alpha"); }, "search on corrupt session accepted");
    std::vector<IndexProgress> events;
    requireFailure([&] { broken.index(root, [&](const IndexProgress& e) { events.push_back(e); }); },
                   "index on corrupt session accepted");
    require(events.empty() && read(corrupt) == "{broken", "corrupt snapshot overwritten");

    const auto documents = root / "documents";
    fs::create_directory(documents);
    write(documents / "a.txt", "alpha\n");
    const auto snapshot = root / "new.json";
    SearchApplication fresh(snapshot);
    fs::create_directory(snapshot.string() + ".tmp");
    requireFailure([&] { fresh.index(documents); }, "first blocked save accepted");
    require(!fs::exists(snapshot) && fresh.search("alpha").empty(), "first failure retained unsaved state");
    fs::remove(snapshot.string() + ".tmp");
    require(fresh.index(documents).metadata.added == 1 && fresh.search("alpha").size() == 1,
            "first save retry failed");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    try {
        const fs::path root = argv[1];
        fs::remove_all(root);
        fs::create_directories(root);
        testSession(root / "session");
        testCorruptionAndFirstSave(root / "failures");
        fs::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
