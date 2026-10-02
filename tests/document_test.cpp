#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

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
        std::ofstream(root / "input" / "a.txt") << "words words\n";
        std::ofstream(root / "input" / "nested" / "자료 구조.md") << "# title\n";
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
            {0, "a\\b\"c\n\t\r\b\f", "자료 구조", 0}};
        mini_lexical_search::saveDocuments(special, root / "escaped.json");
        require(read(root / "escaped.json") ==
                    "[\n  {\"id\": 0, \"path\": \"a\\\\b\\\"c\\u000a\\u0009\\u000d\\u0008\\u000c\", "
                    "\"title\": \"자료 구조\", \"tokenCount\": 0}\n]\n",
                "JSON string escaping failed");
        std::cout << "document_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
