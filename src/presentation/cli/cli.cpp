#include "mini_lexical_search/presentation/cli/cli.hpp"
#include "mini_lexical_search/domain/version.hpp"

#include <exception>
#include <iostream>
#include <sstream>
#include <string>

namespace mini_lexical_search::cli {
namespace {
constexpr std::size_t kMaxMatchingLines = 0;
}  // namespace

void printUsage(const char* program) {
    std::cout
        << "Mini Lexical Search " << mini_lexical_search::versionString() << "\n\n"
        << "사용법:\n"
        << "  " << program << " start\n"
        << "  " << program << " [옵션]\n\n"
        << "옵션:\n"
        << "  -h, --help     도움말 출력\n"
        << "  -v, --version  버전 정보 출력\n\n"
        << "대화형 명령:\n"
        << "  /index [디렉터리]  메타데이터·토큰화·역색인 저장 (기본: ./documents)\n"
        << "  /search <검색어>    모든 매칭 문서·행 검색 (AND 또는 OR 지원)\n"
        << "  /help              도움말 출력\n"
        << "  /exit | /quit      프로그램 종료\n";
}

void runRepl(const char* program, SearchApplication& application) {
    std::cout << "Mini Lexical Search started.\n"
              << "Type '/help' for commands, '/exit' to quit.\n";

    if (!application.ready()) {
        std::cout << "error: cannot load " << application.snapshotPath().string() << ": " << application.loadError() << "\n";
    }

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command;
        if (!(input >> command)) {
            continue;
        }
        std::string arguments;
        std::getline(input >> std::ws, arguments);
        const auto last = arguments.find_last_not_of(" \t\r");
        if (last != std::string::npos) {
            arguments.erase(last + 1);
        }

        if (command == "/index") {
            if (!application.ready()) {
                std::cout << "error: index snapshot is not loaded; fix or remove "
                          << application.snapshotPath().string() << "\n";
                continue;
            }
            try {
                const auto directory = arguments.empty() ? "./documents" : arguments;
                application.index(directory, [](const IndexProgress& event) {
                    switch (event.stage) {
                    case IndexStage::MetadataStarted:
                        std::cout << "updating file metadata..\n" << std::flush;
                        break;
                    case IndexStage::MetadataCompleted:
                        std::cout << "updated file metadata (" << event.elapsedMilliseconds << " ms)\n";
                        std::cout << event.metadata.notUpdated << " not updated, " << event.metadata.updated
                                  << " updated, " << event.metadata.added << " added, "
                                  << event.metadata.deleted << " deleted\n";
                        break;
                    case IndexStage::TokenizationStarted:
                        std::cout << "tokenizing documents..\n" << std::flush;
                        break;
                    case IndexStage::TokenizationCompleted:
                        std::cout << "tokenized " << event.tokens.documents << " documents ("
                                  << event.elapsedMilliseconds << " ms)\n";
                        break;
                    }
                });
            } catch (const std::exception& error) {
                std::cout << "error: indexing failed: " << error.what() << "\n";
            }
            continue;
        }

        if (command == "/search") {
            if (arguments.empty()) {
                std::cout << "Usage: /search <query>\n";
            } else if (!application.ready()) {
                std::cout << "error: index snapshot is not loaded; fix or remove " << application.snapshotPath().string() << "\n";
            } else {
                try {
                    const auto results = application.search(arguments, kMaxMatchingLines);
                    if (results.empty()) std::cout << "No matches.\n";
                    for (std::size_t i = 0; i < results.size(); ++i) {
                        const auto& result = results[i];
                        std::cout << i + 1 << ". " << result.document.title << " (" << result.document.path
                                  << ")    score: " << result.score << "\n";
                        if (!result.warning.empty()) {
                            std::cout << "   warning: " << result.warning << "\n";
                            continue;
                        }
                        for (const auto& match : result.lines) {
                            std::cout << "   " << match.number << ": " << match.text << "\n";
                        }
                        if (result.lines.size() < result.totalMatchingLines) {
                            std::cout << "   + " << result.totalMatchingLines - result.lines.size()
                                      << " more matches\n";
                        }
                    }
                } catch (const std::exception& error) {
                    std::cout << "error: search failed: " << error.what()
                              << "\nUsage: /search <keyword> [AND <keyword> ... | OR <keyword> ...]\n";
                }
            }
            continue;
        }
        if (command == "/help" ||
            command == "/exit" || command == "/quit") {
            if (!arguments.empty()) {
                std::cout << "error: unexpected arguments for " << command << "\n";
                continue;
            }
            if (command == "/exit" || command == "/quit") {
                break;
            }
            printUsage(program);
            continue;
        }
        std::cout << "Unknown command: " << line << "\n";
    }
}

}  // namespace mini_lexical_search::cli
