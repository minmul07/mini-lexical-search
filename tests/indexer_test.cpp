#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "mini_lexical_search/indexer.hpp"

namespace fs = std::filesystem;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "could not read " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

template <typename Operation>
void requireFailure(Operation operation, const std::string& message) {
    bool failed = false;
    try {
        operation();
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed, message);
}

int main(int argc, char* argv[]) {
    try {
        require(argc == 2, "expected a test fixture directory");
        const fs::path root = fs::absolute(argv[1]);
        fs::remove_all(root);
        const auto docs = root / "docs";
        const auto snapshotPath = root / "index.json";
        fs::create_directories(docs);
        std::ofstream(docs / "a.txt") << "자료구조 자료구조\n자료구조\n";
        std::ofstream(docs / "b.md") << "자료구조 알고리즘\n";

        mini_lexical_search::SimpleTokenizer tokenizer;
        mini_lexical_search::IndexSnapshot snapshot;

        // A missing snapshot is not an error; it means nothing was indexed yet.
        const auto missing = mini_lexical_search::loadIndexSnapshot(snapshotPath);
        require(missing.documents.empty() && missing.index.empty(),
                "missing snapshot must load empty");

        // First build: every document is added and tokenized.
        auto update =
            mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.added == 2 && update.metadata.notUpdated == 0 &&
                    update.metadata.updated == 0 && update.metadata.deleted == 0,
                "first build metadata failed");
        require(update.tokens.documents == 2 && update.tokens.tokens == 5,
                "first build tokenization failed");
        require(snapshot.documents.size() == 2 && snapshot.documents[0].id == 0 &&
                    snapshot.documents[1].id == 1,
                "first build document table failed");
        require(snapshot.documents[0].tokenCount == 3 && snapshot.documents[1].tokenCount == 2,
                "tokenCount must be stored on the documents");
        const auto* structure = snapshot.index.find("자료구조");
        require(structure != nullptr && structure->size() == 2,
                "자료구조 must be indexed for both documents");
        require((*structure)[0].documentId == 0 && (*structure)[0].termFrequency == 3 &&
                    (*structure)[0].lines == std::vector<std::uint32_t>{1, 2},
                "first document posting failed");
        require((*structure)[1].documentId == 1 && (*structure)[1].termFrequency == 1 &&
                    (*structure)[1].lines == std::vector<std::uint32_t>{1},
                "second document posting failed");
        require(snapshot.index.find("알고리즘") != nullptr, "알고리즘 must be indexed");

        // The stored file reloads into an equivalent in-memory snapshot.
        const auto reloaded = mini_lexical_search::loadIndexSnapshot(snapshotPath);
        require(reloaded.documents.size() == 2 &&
                    reloaded.documents[1].path == snapshot.documents[1].path,
                "reload must preserve the document table");
        const auto* reloadedStructure = reloaded.index.find("자료구조");
        require(reloadedStructure != nullptr && reloadedStructure->size() == 2 &&
                    (*reloadedStructure)[0].termFrequency == 3 &&
                    (*reloadedStructure)[1].lines == std::vector<std::uint32_t>{1},
                "reload must preserve postings");

        // The serialized format is deterministic.
        const auto tinyPath = root / "tiny.json";
        mini_lexical_search::IndexSnapshot tiny;
        tiny.documents.push_back({0, "a.txt", "a.txt", 1, ""});
        tiny.index.add("a", 0, 1);
        mini_lexical_search::saveIndexSnapshot(tiny.documents, tiny.index, tinyPath);
        require(read(tinyPath) ==
                    "{\n  \"version\": 1,\n  \"documents\": [\n    {\"id\": 0, "
                    "\"path\": \"a.txt\", \"title\": \"a.txt\", \"tokenCount\": 1, "
                    "\"hash\": \"\"}\n  ],\n  \"invertedIndex\": {\n    \"a\": "
                    "[{\"documentId\": 0, \"termFrequency\": 1, \"lines\": [1]}]\n  }\n}\n",
                "snapshot format failed");
        mini_lexical_search::saveIndexSnapshot({}, {}, tinyPath);
        require(read(tinyPath) ==
                    "{\n  \"version\": 1,\n  \"documents\": [],\n  "
                    "\"invertedIndex\": {}\n}\n",
                "empty snapshot format failed");

        // An unchanged directory skips tokenization and keeps the file intact.
        const auto before = read(snapshotPath);
        const auto timestamp = fs::file_time_type::clock::now() - std::chrono::hours(24);
        fs::last_write_time(snapshotPath, timestamp);
        update = mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.notUpdated == 2 && update.metadata.added == 0 &&
                    update.metadata.updated == 0 && update.metadata.deleted == 0,
                "unchanged metadata failed");
        require(update.tokens.documents == 0 && update.tokens.tokens == 0,
                "unchanged documents must not be tokenized");
        require(read(snapshotPath) == before && fs::last_write_time(snapshotPath) == timestamp,
                "unchanged run must not rewrite the snapshot");

        // An edited document is rebuilt from scratch; other postings stay.
        std::ofstream(docs / "a.txt") << "자료구조 변경\n";
        update = mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.updated == 1 && update.metadata.notUpdated == 1 &&
                    update.tokens.documents == 1 && update.tokens.tokens == 2,
                "updated metadata/tokenization failed");
        structure = snapshot.index.find("자료구조");
        require(structure != nullptr && structure->size() == 2 &&
                    (*structure)[0].documentId == 0 && (*structure)[0].termFrequency == 1 &&
                    (*structure)[0].lines == std::vector<std::uint32_t>{1},
                "old postings of the edited document must be discarded");
        const auto* changed = snapshot.index.find("변경");
        require(changed != nullptr && changed->size() == 1 && (*changed)[0].documentId == 0,
                "new tokens of the edited document must be indexed");
        require(snapshot.index.find("알고리즘") != nullptr,
                "postings of unchanged documents must survive");
        require(snapshot.documents[0].tokenCount == 2, "edited tokenCount must be rebuilt");

        // A deleted document drops its postings without tokenization.
        fs::remove(docs / "b.md");
        update = mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.deleted == 1 && update.metadata.updated == 0 &&
                    update.metadata.added == 0,
                "deletion metadata failed");
        require(update.tokens.documents == 0 && update.tokens.tokens == 0,
                "deletion must not tokenize");
        require(snapshot.index.find("알고리즘") == nullptr,
                "deleted document postings must be removed");
        require(snapshot.index.find("자료구조")->size() == 1,
                "other postings must survive a deletion");

        // Tokenless documents are stored with an empty posting contribution.
        std::ofstream(docs / "blank.txt") << "!!!\n";
        update = mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.added == 1 && update.tokens.documents == 1 &&
                    update.tokens.tokens == 0,
                "tokenless document failed");
        require(snapshot.documents.size() == 2 && snapshot.documents[1].tokenCount == 0,
                "tokenless tokenCount must be zero");

        // A failed save must keep both the stored file and the in-memory state.
        const auto goodSnapshot = read(snapshotPath);
        std::ofstream(docs / "a.txt") << "다시\n";
        fs::create_directories(snapshotPath.string() + ".tmp");
        std::ofstream(fs::path(snapshotPath.string() + ".tmp") / "blocker") << "blocked";
        requireFailure(
            [&] {
                mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
            },
            "blocked save must fail");
        require(read(snapshotPath) == goodSnapshot,
                "failed save must not change the stored snapshot");
        require(snapshot.index.find("다시") == nullptr,
                "failed save must roll back the in-memory postings");
        require(snapshot.index.find("변경") != nullptr,
                "rollback must restore the previous postings");
        fs::remove_all(snapshotPath.string() + ".tmp");
        update = mini_lexical_search::updateIndexSnapshot(docs, snapshot, snapshotPath, tokenizer);
        require(update.metadata.updated == 1 && update.tokens.tokens == 1 &&
                    snapshot.index.find("다시") != nullptr &&
                    snapshot.index.find("변경") == nullptr,
                "retry after a failed save must rebuild the postings");

        // Damaged snapshots are rejected instead of being overwritten.
        const auto corruptPath = root / "corrupt.json";
        for (const auto* invalid : {
                 "",
                 "[]",
                 "{}",
                 "{\"version\": 2, \"documents\": [], \"invertedIndex\": {}}",
                 "{\"version\": 1, \"documents\": [], \"invertedIndex\": []}",
                 "{\"version\": 1, \"documents\": [], \"invertedIndex\": {\"a\": []}}",
                 "{\"version\": 1, \"documents\": [],"
                 " \"invertedIndex\": {\"a\": [{\"documentId\": 0,"
                 " \"termFrequency\": 1, \"lines\": [1]}]}}",
                 "{\"version\": 1, \"documents\": [{\"id\": 0, \"path\": \"a\","
                 " \"title\": \"a\", \"tokenCount\": 1}],"
                 " \"invertedIndex\": {\"a\": [{\"documentId\": 1,"
                 " \"termFrequency\": 1, \"lines\": [1]}]}}",
                 "{\"version\": 1}",
                 "{\"version\": 1, \"documents\": [], \"invertedIndex\": {}} trailing",
                 "{\"version\": 1, \"version\": 1, \"documents\": [], \"invertedIndex\": {}}",
                 "{\"version\": 1, \"documents\": {}, \"invertedIndex\": {}}",
                 "{\"version\": 1, \"documents\": [], \"invertedIndex\":"
                 " {\"a\": [{\"documentId\": 0, \"termFrequency\": 1, \"lines\": [0]}]}}",
             }) {
            std::ofstream(corruptPath) << invalid;
            requireFailure([&] { mini_lexical_search::loadIndexSnapshot(corruptPath); },
                           "damaged snapshot accepted");
        }

        std::cout << "indexer_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
