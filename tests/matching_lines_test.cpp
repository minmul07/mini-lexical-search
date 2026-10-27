#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "mini_lexical_search/infrastructure/matching_lines.hpp"

using namespace mini_lexical_search;

void require(bool value) {
    if (!value) throw std::runtime_error("matching lines assertion failed");
}

int main(int argc, char** argv) {
    try {
        require(argc == 2);
        const std::filesystem::path path = argv[1];
        std::filesystem::create_directories(path.parent_path());
        { std::ofstream file(path, std::ios::binary); file << "one\r\n\r\nthree\nfour\nfive"; }
        Document document{8, path.string(), "test.txt", 0, ""};
        const std::vector<std::uint32_t> numbers{1, 2, 3, 4, 5};
        const auto all = readMatchingLines(document, numbers);
        require(all.size() == 5 && all[0].text == "one" && all[1].text.empty() &&
                all[4].text == "five" && all[4].number == 5);
        for (const auto limit : {0u, 1u, 3u, 5u, 10u}) {
            const auto lines = readMatchingLines(document, numbers, limit);
            require(lines.size() == (limit == 0 || limit > 5 ? 5 : limit));
        }
        require(readMatchingLines(document, {3, 5})[1].text == "five");
        require(readMatchingLines(document, {1, 6}, 1).size() == 1);
        require(readMatchingLines(document, {}).empty());
        for (const auto& invalid : std::vector<std::vector<std::uint32_t>>{{0}, {1, 1}, {3, 1}, {6}}) {
            bool failed = false;
            try { readMatchingLines(document, invalid); }
            catch (const std::exception&) { failed = true; }
            require(failed);
        }
        std::filesystem::remove(path);
        require(readMatchingLines(document, {}).empty());
        bool failed = false;
        try { readMatchingLines(document, {1}); }
        catch (const std::exception&) { failed = true; }
        require(failed);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
