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
            for (int row = 0; row < (i == 0 ? 21 : 10); ++row) file << (i == 0 ? "hit 자료구조\n" : "hit\n");
        }
        SearchApplication app(root / "index.bin");
        app.index(root / "docs");
        auto run = [&](const std::string& command, cli::OutputMode mode = cli::OutputMode::Tui) {
            std::ostringstream output;
            cli::handleLine("test", app, command, output, mode);
            return output.str();
        };
        require(run("/next").find("no active search") != std::string::npos, "missing session accepted");
        auto output = run("  hit\t \r");
        require(output.find("=============================\n[0 space.txt] 10+\n1: hit 자료구조\n") != std::string::npos &&
                output.find(" > 11 more\n") != std::string::npos &&
                output.find("[4 space.txt] 10\n") != std::string::npos &&
                output.find("[5 space.txt]") == std::string::npos &&
                output.find("Page 1 | previous: no | next: yes") != std::string::npos,
                "TUI initial output incorrect");
        require(run("/help").find("/more <상대 경로>") != std::string::npos, "TUI help lacks navigation");
        require(run("/help").find("/search") == std::string::npos &&
                run("/help").find("/ 없이 입력") != std::string::npos, "help advertises old search command");
        require(run("/more 0 space.txt").find(" > 1 more\n") != std::string::npos, "space path failed");
        output = run("/more 0 space.txt");
        require(output.find("21: hit 자료구조\n") != std::string::npos && output.find(" more") == std::string::npos,
                "exhausted more displayed");
        require(run("/more 5 space.txt").find("not on the current page") != std::string::npos,
                "off-page path accepted");
        require(run("/next extra").find("unexpected arguments") != std::string::npos, "extra args accepted");
        require(run("/more").find("Usage: /more") != std::string::npos, "empty path accepted");
        require(run("/next").find("[5 space.txt] 10") != std::string::npos, "next command failed");
        output = run("/prev");
        require(output.find("21: hit 자료구조\n") != std::string::npos, "prev command lost expanded data");
        for (const auto mode : {cli::OutputMode::Tui, cli::OutputMode::NoTui}) {
            for (const auto* blank : {"", " \t\r"}) {
                std::ostringstream ignored;
                require(cli::handleLine("test", app, blank, ignored, mode) == cli::LineAction::Ignored &&
                        ignored.str().empty(), "blank input was not ignored");
            }
            require(run("/search", mode).find("Unknown command: /search") != std::string::npos &&
                    run("/search hit", mode).find("Unknown command: /search hit") != std::string::npos,
                    "removed search command accepted");
        }
        require(run("/prev").find("21: hit 자료구조\n") != std::string::npos,
                "ignored input or unknown command cleared the session");
        for (const auto mode : {cli::OutputMode::Tui, cli::OutputMode::NoTui}) {
            for (const auto* query : {"자료구조", "hit AND 자료구조", "hit OR absent"}) {
                output = run(query, mode);
                require(output.find("1: hit 자료구조\n") != std::string::npos,
                        "plain keyword or boolean query failed");
                require((output.find("[4 space.txt]") != std::string::npos) ==
                        (std::string(query) == "hit OR absent"), "boolean query used only part of the input");
            }
            require(run(" \thit\t ", mode).find("[4 space.txt]") != std::string::npos,
                    "surrounding whitespace changed the query");
            output = run("hit 자료구조", mode);
            require(output.find("expected AND or OR") != std::string::npos &&
                    output.find("Usage: <keyword>") != std::string::npos &&
                    output.find("/search") == std::string::npos, "invalid query guidance incorrect");
            for (const auto* keyword : {"search", "index", "help", "more", "next"}) {
                require(run(keyword, mode).find("No matches.") != std::string::npos,
                        "keyword mistaken for a command");
            }
        }
        run("hit 자료구조");
        require(run("/prev").find("no active search") != std::string::npos,
                "invalid search retained session");
        output = run("hit", cli::OutputMode::NoTui);
        require(output.find("21: hit 자료구조\n") != std::string::npos && output.find("[6 space.txt] 10") != std::string::npos &&
                output.find(" more") == std::string::npos && output.find("Page ") == std::string::npos,
                "NoTUI output truncated or paged");
        require(run("/next", cli::OutputMode::NoTui).find("only available in TUI") != std::string::npos,
                "NoTUI navigation accepted");
        require(run("/help", cli::OutputMode::NoTui).find("/more <") == std::string::npos,
                "NoTUI help advertises navigation");
        fs::remove(root / "docs/0 space.txt");
        output = run("hit");
        require(output.find("[0 space.txt] 10+\nwarning: cannot open") != std::string::npos &&
                output.find(" > 21 more") == std::string::npos && output.find("[1 space.txt] 10") != std::string::npos,
                "warning retried or hid subsequent files");
        // Structured feedback drives TUI without parsing the CLI representation.
        {
            std::ofstream leading(root / "docs/ leading.txt");
            for (int row = 0; row < 21; ++row) leading << "hit\n";
        }
        app.index(root / "docs");
        cli::CommandFeedback feedback;
        std::ostringstream stream;
        cli::handleLine("test", app, " hit ", stream, cli::OutputMode::Tui, &feedback);
        require(feedback.display == cli::Display::Results && feedback.newSearch && feedback.query == "hit" &&
                !feedback.elapsed.empty() && feedback.page && feedback.page->files[0].relativePath == " leading.txt",
                "structured search feedback incomplete");
        cli::navigate(app, cli::Navigation::More, " leading.txt", stream, &feedback);
        require(feedback.page && feedback.page->files[0].lines.size() == 20 && !feedback.newSearch,
                "button navigation stripped significant leading whitespace");
        cli::handleLine("test", app, "/more ./ leading.txt", stream, cli::OutputMode::Tui, &feedback);
        require(feedback.page && feedback.page->files[0].lines.size() == 21,
                "explicit relative path did not expand leading-whitespace file");
        cli::handleLine("test", app, "/help", stream, cli::OutputMode::Tui, &feedback);
        require(feedback.display == cli::Display::Text && !feedback.page && !feedback.newSearch,
                "help left stale search feedback");
        cli::handleLine("test", app, "/prev", stream, cli::OutputMode::Tui, &feedback);
        require(feedback.display == cli::Display::Results && feedback.page->files[0].lines.size() == 21,
                "help should preserve expandable search session");
        cli::handleLine("test", app, " ", stream, cli::OutputMode::Tui, &feedback);
        require(feedback.display == cli::Display::Unchanged && !feedback.page, "ignored input has stale feedback");
        app.clearSearch(); // Esc home action.
        require(app.ready(), "home must retain the index");
        cli::handleLine("test", app, "/next", stream, cli::OutputMode::Tui, &feedback);
        require(feedback.display == cli::Display::Text && !feedback.page, "home retained pageable results");
        cli::handleLine("test", app, "hit", stream, cli::OutputMode::Tui, &feedback);
        require(feedback.page && feedback.page->files[0].lines.size() == 10, "search after home retained expansion");
        cli::handleLine("test", app, "hit 자료구조", stream, cli::OutputMode::Tui, &feedback);
        require(feedback.newSearch && feedback.query == "hit 자료구조" &&
                feedback.display == cli::Display::Text && !feedback.page, "failed search feedback incomplete");
        fs::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
