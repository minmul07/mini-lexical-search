#include "mini_lexical_search/presentation/cli/cli.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace mini_lexical_search;
namespace fs = std::filesystem;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    try {
        const fs::path root = fs::absolute(argv[1]);
        fs::remove_all(root);
        fs::create_directories(root / "docs");
        for (int i = 0; i < 7; ++i) {
            std::ofstream file(root / "docs" / (std::to_string(i) + " space.txt"));
            for (int row = 0; row < (i == 0 ? 21 : 10); ++row) file << "hit\n";
        }
        SearchApplication app(root / "index.bin");
        app.index(root / "docs");
        auto run = [&](const std::string& command, cli::OutputMode mode = cli::OutputMode::Tui) {
            std::ostringstream output;
            cli::handleLine("test", app, command, output, mode);
            return output.str();
        };
        require(run("/next").find("no active search") != std::string::npos, "missing session accepted");
        auto output = run("/search hit");
        require(output.find("=============================\n[0 space.txt] 10+\n1: hit\n") != std::string::npos &&
                output.find(" > 11 more\n") != std::string::npos &&
                output.find("[4 space.txt] 10\n") != std::string::npos &&
                output.find("[5 space.txt]") == std::string::npos &&
                output.find("Page 1 | previous: no | next: yes") != std::string::npos,
                "TUI initial output incorrect");
        require(run("/help").find("/more <상대 경로>") != std::string::npos, "TUI help lacks navigation");
        require(run("/more 0 space.txt").find(" > 1 more\n") != std::string::npos, "space path failed");
        output = run("/more 0 space.txt");
        require(output.find("21: hit\n") != std::string::npos && output.find(" more") == std::string::npos,
                "exhausted more displayed");
        require(run("/more 5 space.txt").find("not on the current page") != std::string::npos,
                "off-page path accepted");
        require(run("/next extra").find("unexpected arguments") != std::string::npos, "extra args accepted");
        require(run("/more").find("Usage: /more") != std::string::npos, "empty path accepted");
        require(run("/next").find("[5 space.txt] 10") != std::string::npos, "next command failed");
        output = run("/prev");
        require(output.find("21: hit\n") != std::string::npos, "prev command lost expanded data");
        output = run("/search");
        require(output.find("Usage: /search") != std::string::npos &&
                run("/prev").find("no active search") != std::string::npos, "empty search retained session");
        output = run("/search hit", cli::OutputMode::NoTui);
        require(output.find("21: hit\n") != std::string::npos && output.find("[6 space.txt] 10") != std::string::npos &&
                output.find(" more") == std::string::npos && output.find("Page ") == std::string::npos,
                "NoTUI output truncated or paged");
        require(run("/next", cli::OutputMode::NoTui).find("only available in TUI") != std::string::npos,
                "NoTUI navigation accepted");
        require(run("/help", cli::OutputMode::NoTui).find("/more <") == std::string::npos,
                "NoTUI help advertises navigation");
        fs::remove(root / "docs/0 space.txt");
        output = run("/search hit");
        require(output.find("[0 space.txt] 10+\nwarning: cannot open") != std::string::npos &&
                output.find(" > 21 more") == std::string::npos && output.find("[1 space.txt] 10") != std::string::npos,
                "warning retried or hid subsequent files");
        fs::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
