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
    require(workingJob().label() == "Ready", "missing snapshot must be ready");
    require(app.snapshotPath() == snapshot && app.search("alpha").empty(), "empty session failed");

    std::vector<IndexProgress> events;
    auto collect = [&](const IndexProgress& event) { events.push_back(event); };
    const auto update = app.index(documents, collect);
    requireStages(events);
    require(workingJob().label() == "Ready", "successful indexing must be ready");
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
    require(workingJob().label() == "Failed to load index", "corrupt snapshot must fail the job");
    requireFailure([&] { broken.search("alpha"); }, "search on corrupt session accepted");
    std::vector<IndexProgress> events;
    requireFailure([&] { broken.index(root, [&](const IndexProgress& e) { events.push_back(e); }); },
                   "index on corrupt session accepted");
    require(events.empty() && read(corrupt) == "{broken", "corrupt snapshot overwritten");

    const auto documents = root / "documents";
    fs::create_directories(documents);
    write(documents / "a.txt", "alpha\n");
    fs::create_directory(corrupt.string() + ".tmp");
    requireFailure([&] { broken.rebuildIndex(documents); }, "blocked rebuild accepted");
    require(!broken.ready() && workingJob().label() == "Failed to load index",
            "failed rebuild became ready");
    require(read(corrupt) == "{broken", "failed rebuild overwrote the snapshot");
    fs::remove(corrupt.string() + ".tmp");
    const auto rebuilt = broken.rebuildIndex(documents);
    require(broken.ready() && workingJob().label() == "Ready" && rebuilt.metadata.added == 1 &&
                broken.search("alpha").size() == 1,
            "rebuild did not replace the damaged snapshot");
    require(read(corrupt) != "{broken", "rebuild kept the damaged text");

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

void testRanking(const fs::path& root) {
    const auto documents = root / "documents";
    const auto snapshot = root / "index.json";
    fs::create_directories(documents);
    write(documents / "a_top.txt", "red\nred\nred\nblue blue\n");
    write(documents / "b_early.txt", "same\n");
    write(documents / "c_late.txt", "same\n");
    write(documents / "m_low.txt", "red\n");
    write(documents / "y_top.txt", "same same\n");
    write(documents / "z_mid.txt", "red red\nblue\n");

    SearchApplication app(snapshot);
    const auto indexed = app.index(documents);
    require(indexed.metadata.added == 6, "ranking fixture was not indexed");

    const auto ranked = app.search("red");
    require(ranked.size() == 3 && ranked[0].document.title == "a_top.txt" && ranked[0].score == 3 &&
                ranked[0].lines.size() == 3 && ranked[0].lines[0].text == "red" &&
                ranked[1].document.title == "z_mid.txt" && ranked[1].score == 2 &&
                ranked[2].document.title == "m_low.txt" && ranked[2].score == 1 &&
                ranked[0].document.id < ranked[2].document.id && ranked[2].document.id < ranked[1].document.id,
            "single keyword ranking failed");

    const auto limited = app.search("red", 1, 2);
    require(limited.size() == 2 && limited[0].document.title == "a_top.txt" &&
                limited[0].lines.size() == 1 && limited[0].totalMatchingLines == 3 &&
                limited[0].lines[0].text == "red" && limited[1].document.title == "z_mid.txt" &&
                limited[1].warning.empty(),
            "maxMatchingLines and topK failed");
    require(app.search("red", 0, 0).empty(), "topK zero returned matches");
    require(app.search("red", 0, 1).size() == 1 && app.search("red", 0, 1)[0].document.title == "a_top.txt",
            "topK one failed");
    const auto all = app.search("red", 0, 3);
    require(all.size() == 3 && all[0].score == 3 && all[2].score == 1, "topK equal to candidates failed");
    const auto more = app.search("red", 0, 100);
    require(more.size() == 3 && more[0].document.title == "a_top.txt" &&
                more[2].document.title == "m_low.txt",
            "topK above candidates failed");

    const auto both = app.search("red AND blue");
    require(both.size() == 2 && both[0].document.title == "a_top.txt" && both[0].score == 5 &&
                both[1].document.title == "z_mid.txt" && both[1].score == 3,
            "AND ranking failed");
    const auto either = app.search("red OR blue");
    require(either.size() == 3 && either[0].score == 5 && either[1].score == 3 && either[2].score == 1,
            "OR ranking failed");
    const auto repeatedAnd = app.search("red AND red");
    const auto repeatedOr = app.search("red OR red");
    require(repeatedAnd.size() == ranked.size() && repeatedOr.size() == ranked.size(),
            "duplicate terms changed the result count");
    for (std::size_t i = 0; i < ranked.size(); ++i) {
        require(repeatedAnd[i].score == ranked[i].score && repeatedAnd[i].document.id == ranked[i].document.id &&
                    repeatedOr[i].score == ranked[i].score && repeatedOr[i].document.id == ranked[i].document.id,
                "duplicate terms changed the score");
    }

    const auto ties = app.search("same", 0, 2);
    require(ties.size() == 2 && ties[0].document.title == "y_top.txt" && ties[0].score == 2 &&
                ties[1].document.title == "b_early.txt" && ties[1].score == 1,
            "tie at the topK boundary failed");
    const auto allTies = app.search("same");
    require(allTies.size() == 3 && allTies[1].document.title == "b_early.txt" &&
                allTies[2].document.title == "c_late.txt" &&
                allTies[1].document.id < allTies[2].document.id,
            "equal scores were not ordered by document id");

    const auto saved = read(snapshot);
    const auto modified = fs::last_write_time(snapshot);
    SearchApplication reopened(snapshot);
    const auto reloaded = reopened.search("red");
    require(reloaded.size() == ranked.size() && reloaded[0].score == 3 && reloaded[0].lines.size() == 3 &&
                reloaded[2].document.title == "m_low.txt",
            "reloaded ranking changed");
    require(read(snapshot) == saved && fs::last_write_time(snapshot) == modified,
            "ranking search changed the snapshot");

    fs::remove(documents / "m_low.txt");
    const auto skipped = app.search("red", 0, 2);
    require(skipped.size() == 2 && skipped[0].document.title == "a_top.txt" && skipped[0].warning.empty() &&
                skipped[1].document.title == "z_mid.txt" && skipped[1].warning.empty(),
            "document outside topK was returned or read");

    fs::remove(documents / "a_top.txt");
    const auto failed = app.search("red", 0, 1);
    require(failed.size() == 1 && failed[0].document.title == "a_top.txt" && failed[0].score == 3 &&
                failed[0].lines.empty() && failed[0].totalMatchingLines == 3 &&
                failed[0].warning.find("cannot open") != std::string::npos,
            "read failure replaced the selected document");
    require(read(snapshot) == saved && fs::last_write_time(snapshot) == modified,
            "failed read changed the snapshot");
}

void testDefaultTopK(const fs::path& root) {
    const auto documents = root / "documents";
    const auto snapshot = root / "index.json";
    fs::create_directories(documents);
    const std::vector<std::pair<std::string, int>> files{
        {"n00.txt", 3}, {"n01.txt", 1}, {"n02.txt", 12}, {"n03.txt", 2},
        {"n04.txt", 11}, {"n05.txt", 10}, {"n06.txt", 9}, {"n07.txt", 8},
        {"n08.txt", 7}, {"n09.txt", 6}, {"n10.txt", 5}, {"n11.txt", 4}};
    for (const auto& [title, score] : files) {
        std::string text;
        for (int count = 0; count < score; ++count) {
            if (count != 0) text += ' ';
            text += "hit";
        }
        write(documents / title, text + "\n");
    }

    SearchApplication app(snapshot);
    require(app.index(documents).metadata.added == 12, "default topK fixture was not indexed");
    const auto results = app.search("hit");
    const std::vector<std::string> expected{
        "n02.txt", "n04.txt", "n05.txt", "n06.txt", "n07.txt", "n08.txt",
        "n09.txt", "n10.txt", "n11.txt", "n00.txt"};
    require(results.size() == expected.size(), "default search did not return 10 results");
    for (std::size_t i = 0; i < results.size(); ++i) {
        require(results[i].document.title == expected[i] &&
                    results[i].score == static_cast<double>(12 - i) && results[i].warning.empty() &&
                    results[i].lines.size() == 1,
                "default topK order failed");
    }
    bool followsIdOrder = true;
    for (std::size_t i = 1; i < results.size(); ++i) {
        if (results[i].document.id < results[i - 1].document.id) followsIdOrder = false;
    }
    require(!followsIdOrder, "default results followed document id order");

    fs::remove(documents / "n01.txt");
    fs::remove(documents / "n03.txt");
    const auto skipped = app.search("hit");
    require(skipped.size() == 10 && skipped.front().document.title == "n02.txt" &&
                skipped.back().document.title == "n00.txt",
            "missing documents outside topK changed the result");
    for (const auto& result : skipped) {
        require(result.warning.empty(), "document outside topK was opened");
    }

    fs::remove(documents / "n02.txt");
    const auto failed = app.search("hit", 0, 1);
    require(failed.size() == 1 && failed[0].document.title == "n02.txt" &&
                failed[0].warning.find("cannot open") != std::string::npos,
            "default topK replaced a document that could not be read");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    try {
        const fs::path root = argv[1];
        fs::remove_all(root);
        fs::create_directories(root);
        testSession(root / "session");
        testRanking(root / "ranking");
        testDefaultTopK(root / "default-top-k");
        testCorruptionAndFirstSave(root / "failures");
        fs::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
