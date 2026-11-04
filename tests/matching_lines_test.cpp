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
        MatchingReadPosition position;
        const std::vector<std::uint32_t> sparse{1, 3, 5};
        std::size_t cursor = 0;
        std::vector<MatchingLine> resumed;
        auto source = [&]() -> std::uint32_t { return cursor < sparse.size() ? sparse[cursor++] : 0; };
        auto sink = [&](const MatchingLine& line) { resumed.push_back(line); };
        require(readMatchingLineRange(document, position, source, 1, sink).empty());
        require(position.line == 1 && position.offset == 5);
        require(readMatchingLineRange(document, position, source, 1, sink).empty());
        require(position.line == 3 && resumed.back().text == "three");
        require(readMatchingLineRange(document, position, source, 10, sink).empty());
        require(resumed.size() == 3 && resumed.back().text == "five" && position.line == 5);
        MatchingReadPosition missing;
        std::uint32_t target = 6;
        require(readMatchingLineRange(document, missing, [&] { return target; }, 1, sink)
                    .find("indexed lines are missing") != std::string::npos);
        MatchingReadPosition throwing;
        bool sinkFailed = false;
        try {
            readMatchingLineRange(document, throwing, [] { return 1u; }, 1,
                [](const MatchingLine&) { throw std::runtime_error("sink"); });
        } catch (const std::runtime_error& error) { sinkFailed = std::string(error.what()) == "sink"; }
        require(sinkFailed);
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
