#include "mini_lexical_search/presentation/cli/cli.hpp"
#include "mini_lexical_search/application/log.hpp"
#include "mini_lexical_search/domain/version.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <stdexcept>

namespace mini_lexical_search::cli {
namespace {
constexpr const char* kSeparator = "=============================";

void writeGroup(std::ostream& output, const SearchResult& result) {
    output << '[' << result.relativePath << "] "
           << (result.totalMatchingLines > 10 ? "10+" : std::to_string(result.totalMatchingLines)) << '\n';
}

void writePage(std::ostream& output, const SearchPage& page) {
    if (page.files.empty()) {
        output << "No matches.\n";
    } else {
        output << kSeparator << '\n';
        for (const auto& result : page.files) {
            writeGroup(output, result);
            for (const auto& line : result.lines) output << line.number << ": " << line.text << '\n';
            if (!result.warning.empty()) output << "warning: " << result.warning << '\n';
            else if (result.lines.size() < result.totalMatchingLines) {
                output << " > " << result.totalMatchingLines - result.lines.size() << " more\n";
            }
        }
        output << kSeparator << '\n';
    }
    output << "Page " << page.number << " | previous: " << (page.hasPrevious ? "yes" : "no")
           << " | next: " << (page.hasNext ? "yes" : "no") << '\n';
}

void writeIndexProgress(std::ostream& output, const IndexProgress& event) {
    switch (event.stage) {
    case IndexStage::MetadataStarted:
        output << "updating file metadata..\n" << std::flush;
        break;
    case IndexStage::MetadataCompleted:
        output << "updated file metadata (" << formatElapsedMilliseconds(event.elapsedMicroseconds)
               << " ms)\n";
        output << event.metadata.notUpdated << " not updated, " << event.metadata.updated
               << " updated, " << event.metadata.added << " added, "
               << event.metadata.deleted << " deleted\n";
        break;
    case IndexStage::TokenizationStarted:
        output << "tokenizing documents..\n" << std::flush;
        break;
    case IndexStage::TokenizationCompleted:
        output << "tokenized " << event.tokens.documents << " documents ("
               << formatElapsedMilliseconds(event.elapsedMicroseconds) << " ms)\n";
        break;
    case IndexStage::MetadataProgress:
    case IndexStage::TokenizationProgress:
    case IndexStage::SaveStarted:
    case IndexStage::SaveCompleted:
        // CLI는 기존 단계 요약만 출력하고, 세부 진행은 TUI가 표시한다.
        break;
    }
}

enum class RebuildChoice { Ready, Declined, Failed };

RebuildChoice recreateIndex(SearchApplication& application) {
    while (true) {
        std::cout << "Failed to load the index. Recreate the index? (Y/n) " << std::flush;
        std::string answer;
        if (!std::getline(std::cin, answer)) {
            return RebuildChoice::Declined;
        }
        if (!answer.empty() && answer.back() == '\r') {
            answer.pop_back();
        }
        if (answer.empty() || answer == "Y" || answer == "y") {
            try {
                application.rebuildIndex("./documents", [](const IndexProgress& event) {
                    writeIndexProgress(std::cout, event);
                });
                return RebuildChoice::Ready;
            } catch (const std::exception& error) {
                std::cout << "error: indexing failed: " << error.what() << "\n";
                return RebuildChoice::Failed;
            }
        }
        if (answer == "n" || answer == "N") {
            return RebuildChoice::Declined;
        }
    }
}

void writeUsage(std::ostream& output, const char* program, OutputMode mode = OutputMode::NoTui) {
    output
        << "Mini Lexical Search " << mini_lexical_search::versionString() << "\n\n"
        << "사용법:\n"
        << "  " << program << " start\n"
        << "  " << program << " start --no-tui\n"
        << "  " << program << " [옵션]\n\n"
        << "옵션:\n"
        << "  -h, --help     도움말 출력\n"
        << "  -v, --version  버전 정보 출력\n"
        << "  --no-tui       TUI 없이 CLI로 실행\n\n"
        << "대화형 명령:\n"
        << "  /index [디렉터리]  메타데이터·토큰화·역색인 저장 (기본: ./documents)\n"
        << "  <검색어>           / 없이 입력하여 파일별 검색 (디렉터리·이름순, AND 또는 OR)\n"
        << (mode == OutputMode::Tui
                ? "  ↑↓ 방향키          검색창·결과 이동 및 선택\n  Space / Enter     선택한 더보기·이전·다음 버튼 실행\n  마우스 클릭        더보기·이전·다음 버튼 실행\n  PgUp / PgDn / 휠   결과 스크롤\n  Esc               홈으로 복귀 (종료하지 않음)\n  Ctrl+C            프로그램 종료\n"
                : "  --no-tui 검색은 모든 파일·매칭 행을 출력합니다.\n")
        << "  /help              도움말 출력\n"
        << "  /exit | /quit      프로그램 종료\n";
}

}  // namespace

void printUsage(const char* program) {
    writeUsage(std::cout, program);
}

void navigate(SearchApplication& application, Navigation navigation, const std::string& path,
              std::ostream& output, CommandFeedback* feedback) {
    if (feedback) { *feedback = {}; feedback->display = Display::Text; }
    try {
        auto page = navigation == Navigation::Next ? application.nextPage()
                        : navigation == Navigation::Previous ? application.previousPage()
                        : application.expandFile(path);
        if (!feedback) writePage(output, page);
        if (feedback) { feedback->display = Display::Results; feedback->page = std::move(page); }
        const std::string action = navigation == Navigation::Next ? "next"
                                  : navigation == Navigation::Previous ? "prev" : "more";
        appendLog("search navigation " + action + " " + path + " page=" + std::to_string(page.number));
    } catch (const std::exception& error) {
        output << "error: " << error.what() << '\n';
    }
}

LineAction handleLine(const char* program, SearchApplication& application, std::string_view line,
                      std::ostream& output, OutputMode mode, CommandFeedback* feedback,
                      const IndexDispatcher& indexDispatcher) {
    if (feedback) *feedback = {};
    const auto first = line.find_first_not_of(" \t\r\n\v\f");
    if (first == std::string_view::npos) {
        return LineAction::Ignored;
    }
    line = line.substr(first, line.find_last_not_of(" \t\r\n\v\f") - first + 1);
    if (feedback) feedback->display = Display::Text;
    std::istringstream input{std::string(line)};
    std::string command;
    if (!(input >> command)) {
        return LineAction::Ignored;
    }
    std::string arguments;
    std::getline(input >> std::ws, arguments);
    const auto last = arguments.find_last_not_of(" \t\r");
    if (last != std::string::npos) {
        arguments.erase(last + 1);
    }

    if (command == "/index") {
        if (!application.ready()) {
            output << "error: index snapshot is not loaded; fix or remove "
                   << application.snapshotPath().string() << "\n";
            return LineAction::Ran;
        }
        const auto directory = arguments.empty() ? "./documents" : arguments;
        if (indexDispatcher) {
            indexDispatcher(directory);
            if (feedback) {
                feedback->display = Display::Unchanged;
                feedback->indexStarted = true;
            }
            return LineAction::Ran;
        }
        try {
            application.index(directory, [&output](const IndexProgress& event) {
                writeIndexProgress(output, event);
            });
        } catch (const std::exception& error) {
            output << "error: indexing failed: " << error.what() << "\n";
        }
        return LineAction::Ran;
    }

    if (command.front() != '/') {
        const std::string query(line);
        if (feedback) { feedback->newSearch = true; feedback->query = query; }
        if (!application.ready()) {
            output << "error: index snapshot is not loaded; fix or remove "
                   << application.snapshotPath().string() << "\n";
        } else {
            try {
                using Clock = std::chrono::steady_clock;
                const auto started = Clock::now();
                std::ostringstream summaries;
                std::size_t fileCount = 0;
                const auto summarize = [&](const SearchResult& result) {
                    summaries << ' ' << ++fileCount << " id=" << result.document.id
                              << " matchingLines=" << result.totalMatchingLines;
                };
                std::string elapsed;
                const auto writeTime = [&] {
                    elapsed = formatElapsedMilliseconds(
                        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - started).count());
                    if (mode != OutputMode::Tui || !feedback)
                        output << "searched (" << elapsed << " ms)\n";
                };
                if (mode == OutputMode::Tui) {
                    auto page = application.search(query);
                    writeTime();
                    for (const auto& result : page.files) summarize(result);
                    if (!feedback) writePage(output, page);
                    if (feedback) {
                        feedback->display = Display::Results;
                        feedback->page = std::move(page);
                        feedback->elapsed = elapsed;
                    }
                } else {
                    application.visitAllResults(query,
                        [&](const SearchResult& result) {
                            if (fileCount == 0) {
                                writeTime();
                                output << kSeparator << '\n';
                            }
                            summarize(result);
                            writeGroup(output, result);
                        },
                        [&](const MatchingLine& match) { output << match.number << ": " << match.text << '\n'; },
                        [&](const std::string& warning) { output << "warning: " << warning << '\n'; });
                    if (fileCount == 0) { writeTime(); output << "No matches.\n"; }
                    else output << kSeparator << '\n';
                }
                appendLog("search " + query + " (" + elapsed + " ms) results=" +
                          std::to_string(fileCount) + summaries.str());
            } catch (const std::exception& error) {
                output << "error: search failed: " << error.what()
                       << "\nUsage: <keyword> [AND <keyword> ... | OR <keyword> ...]\n";
            }
        }
        return LineAction::Ran;
    }
    if (command == "/help" || command == "/exit" || command == "/quit") {
        if (!arguments.empty()) {
            output << "error: unexpected arguments for " << command << "\n";
            return LineAction::Ran;
        }
        if (command == "/exit" || command == "/quit") {
            return LineAction::Quit;
        }
        writeUsage(output, program, mode);
        return LineAction::Ran;
    }
    output << "Unknown command: " << line << "\n";
    return LineAction::Ran;
}

int runRepl(const char* program, SearchApplication& application) {
    if (!application.ready()) {
        const auto choice = recreateIndex(application);
        if (choice == RebuildChoice::Declined) {
            return 0;
        }
        if (choice == RebuildChoice::Failed) {
            return 1;
        }
    }

    std::cout << "Mini Lexical Search started.\n"
              << "Type text to search, '/help' for commands, '/exit' to quit.\n";

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        if (handleLine(program, application, line, std::cout) == LineAction::Quit) {
            break;
        }
    }
    return 0;
}

}  // namespace mini_lexical_search::cli
