#include <filesystem>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "mini_lexical_search/document.hpp"

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
        fs::create_directories(root / "input" / "nested");
        std::ofstream(root / "input" / "a.txt") << "abc";
        std::ofstream(root / "input" / "nested" / "자료 구조.md");
        std::ofstream(root / "input" / "ignored.json") << "{}";
        std::ofstream(root / "input" / "ignored.TXT") << "ignored";
        fs::create_symlink(root / "input" / "a.txt", root / "input" / "alias.txt");
        fs::create_directory_symlink(root / "input", root / "input" / "loop");

        const auto documents = mini_lexical_search::collectDocuments(root / "input");
        require(documents.size() == 2, "extension/symlink filtering failed");
        require(documents[0].id == 0 && documents[1].id == 1, "IDs must start at zero");
        require(documents[0].title == "a.txt" && documents[1].title == "자료 구조.md",
                "titles must use filenames and paths must be sorted");
        require(documents[0].path == (root / "input" / "a.txt").generic_string(),
                "paths must be absolute");
        for (const auto& document : documents) {
            require(document.tokenCount == 0, "tokenCount must stay zero");
        }
        require(documents[0].hash ==
                    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                "SHA-256 known vector failed");
        require(documents[1].hash ==
                    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                "empty file SHA-256 failed");
        const auto repeated = mini_lexical_search::collectDocuments(root / "input");
        require(repeated[1].path == documents[1].path && repeated[1].id == documents[1].id,
                "repeated indexing must be deterministic");

        const auto output = root / "output" / "documents.json";
        mini_lexical_search::saveDocuments(documents, output);
        const auto original = read(output);
        require(!fs::exists(output.string() + ".tmp"), "temporary file left after save");
        requireFailure([&] { mini_lexical_search::collectDocuments(root / "missing"); },
                       "missing input must fail");
        requireFailure([&] { mini_lexical_search::collectDocuments(root / "input" / "a.txt"); },
                       "file input must fail");

        // A blocked temporary path must not replace the last successful document table.
        fs::create_directories(output.string() + ".tmp");
        std::ofstream(fs::path(output.string() + ".tmp") / "blocker") << "blocked";
        requireFailure([&] { mini_lexical_search::saveDocuments({}, output); },
                       "unwritable temporary file must fail");
        require(read(output) == original, "failed save changed existing output");
        require(fs::exists(fs::path(output.string() + ".tmp") / "blocker"),
                "failed reservation removed another writer's staging directory");
        fs::remove_all(output.string() + ".tmp");

        const auto blockedTarget = root / "blocked_target";
        fs::create_directory(blockedTarget);
        std::ofstream(blockedTarget / "existing") << "preserve";
        requireFailure([&] { mini_lexical_search::saveDocuments(documents, blockedTarget); },
                       "replacing a directory with a JSON file must fail");
        require(read(blockedTarget / "existing") == "preserve",
                "failed replacement changed the target");
        require(!fs::exists(blockedTarget.string() + ".tmp"),
                "failed replacement left a staging directory");

        fs::create_directory(root / "empty");
        const auto empty = mini_lexical_search::collectDocuments(root / "empty");
        require(empty.empty(), "empty directory must produce no documents");
        mini_lexical_search::saveDocuments(empty, output);
        require(read(output) == "[\n]\n", "re-indexing must replace old documents");
        mini_lexical_search::saveDocuments(documents, output);

        // Verify all JSON escape classes, plus non-ASCII UTF-8 text.
        const std::vector<mini_lexical_search::Document> special = {
            {0, "a\\b\"c\n\t\r\b\f", "자료 구조", 0, documents[0].hash}};
        mini_lexical_search::saveDocuments(special, root / "escaped.json");
        require(read(root / "escaped.json") ==
                    "[\n  {\"id\": 0, \"path\": \"a\\\\b\\\"c\\u000a\\u0009\\u000d\\u0008\\u000c\", "
                    "\"title\": \"자료 구조\", \"tokenCount\": 0, \"hash\": \"" +
                    documents[0].hash + "\"}\n]\n",
                "JSON string escaping failed");
        const auto decoded = mini_lexical_search::loadDocuments(root / "escaped.json");
        require(decoded[0].path == special[0].path && decoded[0].title == special[0].title,
                "JSON escape round trip failed");

        fs::create_directory(root / "large");
        std::string bytes(150000, 'x');
        bytes[65536] = '\0';
        std::ofstream(root / "large" / "large.txt", std::ios::binary).write(bytes.data(), bytes.size());
        mini_lexical_search::saveDocuments(mini_lexical_search::collectDocuments(root / "large"),
                                           root / "large.json");

        const auto live = root / "live";
        const auto index = root / "index.json";
        fs::create_directory(live);
        std::ofstream(live / "a.txt") << "old";
        std::ofstream(live / "b.md") << "delete";
        std::ofstream(live / "keep.txt") << "unchanged";
        auto stats = mini_lexical_search::indexDocuments(live, index);
        require(stats.added == 3 && stats.updated == 0 && stats.deleted == 0 && stats.notUpdated == 0,
                "first indexing stats failed");
        const auto before = read(index);
        const auto timestamp = fs::file_time_type::clock::now() - std::chrono::hours(24);
        fs::last_write_time(index, timestamp);
        stats = mini_lexical_search::indexDocuments(live, index);
        require(stats.notUpdated == 3 && stats.added == 0 && stats.updated == 0 && stats.deleted == 0,
                "unchanged indexing stats failed");
        require(read(index) == before && fs::last_write_time(index) == timestamp,
                "unchanged index must not be rewritten");

        std::ofstream(live / "a.txt") << "new";
        fs::remove(live / "b.md");
        std::ofstream(live / "c.txt") << "unchanged";
        stats = mini_lexical_search::indexDocuments(live, index);
        require(stats.notUpdated == 1 && stats.updated == 1 && stats.added == 1 && stats.deleted == 1,
                "mixed indexing stats failed");
        const auto changed = mini_lexical_search::loadDocuments(index);
        require(changed[0].id == 0 && changed[1].id == 3 && changed[2].id == 2,
                "existing IDs must remain stable; new IDs must not collide");
        require(changed[2].hash == changed[1].hash && changed[2].tokenCount == 1,
                "same-content documents must remain distinct");

        requireFailure([&] { mini_lexical_search::indexDocuments(root / "missing", index); },
                       "invalid input must fail before modifying stored JSON");
        const auto validIndex = read(index);
        std::ofstream(index) << "invalid JSON";
        requireFailure([&] { mini_lexical_search::indexDocuments(live, index); },
                       "corrupted existing JSON must not be silently overwritten");
        require(read(index) == "invalid JSON", "corrupted index was overwritten");
        std::ofstream(index) << validIndex;
        fs::remove_all(live);
        fs::create_directory(live);
        stats = mini_lexical_search::indexDocuments(live, index);
        require(stats.deleted == 3 && stats.added == 0 && stats.updated == 0 && stats.notUpdated == 0,
                "empty input must delete all previous records");
        fs::last_write_time(index, timestamp);
        stats = mini_lexical_search::indexDocuments(live, index);
        require(stats.notUpdated == 0 && stats.updated == 0 && stats.added == 0 && stats.deleted == 0 &&
                    fs::last_write_time(index) == timestamp,
                "empty unchanged index must not be rewritten");

        // Tokenization reads only queued files, records original line numbers,
        // and fills tokenCount. Unchanged files must stay gated out.
        mini_lexical_search::SimpleTokenizer tokenizer;
        const auto tokenRoot = root / "tokenize";
        const auto tokenIndex = root / "tokenize.json";
        fs::create_directory(tokenRoot);
        std::ofstream(tokenRoot / "sample.txt")
            << "First line\n"
               "\n"
               "  자료구조 자료구조  \n"
               "C++에서 자료구조를 배운다\n";
        auto work = mini_lexical_search::prepareIndex(tokenRoot, tokenIndex);
        require(work.changed && work.toTokenize.size() == 1 && work.stats.added == 1,
                "new document must be queued for tokenization");
        std::vector<std::pair<std::uint32_t, std::string>> emitted;
        const mini_lexical_search::TokenSink sink =
            [&emitted](mini_lexical_search::DocumentId id, std::uint32_t line,
                       const std::string& token) {
                require(id == 0, "token sink received the wrong document");
                emitted.emplace_back(line, token);
            };
        const auto tokenStats =
            mini_lexical_search::tokenizeDocuments(work.documents, work.toTokenize, tokenizer, sink);
        require(tokenStats.documents == 1 && tokenStats.tokens == 7,
                "tokenization must count tokens across lines");
        require(work.documents[0].tokenCount == 7, "tokenCount must be stored on the document");
        const std::vector<std::pair<std::uint32_t, std::string>> expectedTokens = {
            {1, "first"}, {1, "line"}, {3, "자료구조"}, {3, "자료구조"},
            {4, "c에서"}, {4, "자료구조를"}, {4, "배운다"}};
        require(emitted == expectedTokens, "tokens must carry their original 1-based line numbers");

        mini_lexical_search::saveDocuments(work.documents, tokenIndex);
        const auto tokenBefore = read(tokenIndex);
        const auto tokenStamp = fs::file_time_type::clock::now() - std::chrono::hours(24);
        fs::last_write_time(tokenIndex, tokenStamp);
        auto again = mini_lexical_search::prepareIndex(tokenRoot, tokenIndex);
        require(!again.changed && again.toTokenize.empty(), "unchanged document must be gated out");
        const auto againStats =
            mini_lexical_search::tokenizeDocuments(again.documents, again.toTokenize, tokenizer, {});
        require(againStats.documents == 0 && againStats.tokens == 0,
                "gated tokenization must not read files");
        require(read(tokenIndex) == tokenBefore && fs::last_write_time(tokenIndex) == tokenStamp,
                "gated tokenization must not rewrite the table");

        std::ofstream(tokenRoot / "sample.txt") << "changed";
        auto modified = mini_lexical_search::prepareIndex(tokenRoot, tokenIndex);
        require(modified.changed && modified.stats.updated == 1 && modified.toTokenize.size() == 1,
                "edited document must be queued again");

        // Tokenless files produce no tokens but are still recorded with tokenCount 0.
        const auto emptyRoot = root / "empty_tokens";
        const auto emptyIndex = root / "empty_tokens.json";
        fs::create_directory(emptyRoot);
        std::ofstream(emptyRoot / "blank.txt") << "   \n!!!\n";
        auto emptyWork = mini_lexical_search::prepareIndex(emptyRoot, emptyIndex);
        const auto emptyStats =
            mini_lexical_search::tokenizeDocuments(emptyWork.documents, emptyWork.toTokenize, tokenizer, {});
        require(emptyStats.documents == 1 && emptyStats.tokens == 0 &&
                    emptyWork.documents[0].tokenCount == 0,
                "tokenless files must produce zero tokens");
        mini_lexical_search::saveDocuments(emptyWork.documents, emptyIndex);
        auto emptyAgain = mini_lexical_search::prepareIndex(emptyRoot, emptyIndex);
        require(!emptyAgain.changed && emptyAgain.toTokenize.empty(),
                "tokenless unchanged document must stay gated by hash, not tokenCount");

        auto legacy = documents;
        for (auto& document : legacy) document.hash.clear();
        mini_lexical_search::saveDocuments(legacy, index);
        auto legacyJson = read(index);
        const std::string hashField = ", \"hash\": \"\"";
        std::size_t found = 0;
        while ((found = legacyJson.find(hashField)) != std::string::npos) {
            legacyJson.erase(found, hashField.size());
        }
        std::ofstream(index) << legacyJson;
        stats = mini_lexical_search::indexDocuments(root / "input", index);
        require(stats.updated == 2 && stats.notUpdated == 0 && stats.added == 0 && stats.deleted == 0,
                "legacy records must be updated with hashes");

        const auto parserFixture = root / "parser.json";
        auto duplicates = documents;
        duplicates.push_back(documents[0]);
        mini_lexical_search::saveDocuments(duplicates, parserFixture);
        requireFailure([&] { mini_lexical_search::loadDocuments(parserFixture); },
                       "duplicate paths and IDs must be rejected");
        auto badHash = documents;
        badHash[0].hash = "invalid";
        mini_lexical_search::saveDocuments(badHash, parserFixture);
        requireFailure([&] { mini_lexical_search::loadDocuments(parserFixture); },
                       "invalid hash must be rejected");
        std::ofstream(parserFixture) <<
            "[{\"id\":0,\"path\":\"\\u0061\\ud83d\\ude00\",\"title\":\"\\uD55C\",\"tokenCount\":0}]";
        const auto unicode = mini_lexical_search::loadDocuments(parserFixture);
        require(unicode[0].path == "a😀" && unicode[0].title == "한", "unicode JSON decode failed");
        for (const auto* invalid : {
                 "[] trailing", "[{}]", "[", "[0]", "[{\"id\":00}]",
                 "[{\"id\":4294967296}]", "[{\"id\":-1}]",
                 "[{\"path\":\"\\ud800\"}]", "[{\"id\":0,\"id\":1}]"}) {
            std::ofstream(parserFixture) << invalid;
            requireFailure([&] { mini_lexical_search::loadDocuments(parserFixture); },
                           "malformed JSON accepted");
        }
        std::cout << "document_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
