#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "mini_lexical_search/document.hpp"
#include "mini_lexical_search/indexer.hpp"
#include "mini_lexical_search/version.hpp"

namespace {

constexpr const char* kIndexPath = ".data/index.json";

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
        << "  /search <검색어>    색인된 문서 검색 (미구현)\n"
        << "  /help              도움말 출력\n"
        << "  /exit | /quit      프로그램 종료\n";
}

// Metadata, tokenization, and the persisted inverted index are available; the
// query side (search) remains P0 work.
void runRepl(const char* program) {
    std::cout << "Mini Lexical Search started.\n"
              << "Type '/help' for commands, '/exit' to quit.\n";

    mini_lexical_search::IndexSnapshot snapshot;
    bool indexLoaded = true;
    try {
        snapshot = mini_lexical_search::loadIndexSnapshot(kIndexPath);
    } catch (const std::exception& error) {
        indexLoaded = false;
        std::cout << "error: cannot load " << kIndexPath << ": " << error.what() << "\n";
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
            if (!indexLoaded) {
                std::cout << "error: index snapshot is not loaded; fix or remove "
                          << kIndexPath << "\n";
                continue;
            }
            try {
                const auto directory = arguments.empty() ? "./documents" : arguments;
                mini_lexical_search::SimpleTokenizer tokenizer;

                std::cout << "updating file metadata..\n" << std::flush;
                const auto metadataStarted = std::chrono::steady_clock::now();
                auto work = mini_lexical_search::prepareIndex(directory, snapshot.documents);
                const auto metadataElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - metadataStarted).count();
                std::cout << "updated file metadata (" << metadataElapsed << " ms)\n";
                std::cout << work.stats.notUpdated << " not updated, " << work.stats.updated
                          << " updated, " << work.stats.added << " added, "
                          << work.stats.deleted << " deleted\n";

                std::cout << "tokenizing documents..\n" << std::flush;
                const auto tokenizeStarted = std::chrono::steady_clock::now();
                const auto tokenStats =
                    mini_lexical_search::applyIndexWork(work, snapshot.index, tokenizer);
                const auto tokenizeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - tokenizeStarted).count();
                std::cout << "tokenized " << tokenStats.documents << " documents ("
                          << tokenizeElapsed << " ms)\n";

                if (work.changed) {
                    mini_lexical_search::commitIndexSnapshot(
                        std::move(work.documents), snapshot, kIndexPath);
                }
            } catch (const std::exception& error) {
                std::cout << "error: indexing failed: " << error.what() << "\n";
            }
            continue;
        }

        if (command == "/search") {
            if (arguments.empty()) {
                std::cout << "Usage: /search <query>\n";
            } else {
                std::cout << "(not implemented yet) Search arrives in P0.\n";
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

}  // namespace

int main(int argc, char* argv[]) {
    bool start = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            std::cout << "mini_lexical_search 버전 " << mini_lexical_search::versionString() << "\n";
            return 0;
        }
        if (arg != "start" || start) {
            std::cerr << "error: unexpected argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
        start = true;
    }

    if (!start) {
        printUsage(argv[0]);
        return 1;
    }

    runRepl(argv[0]);
    return 0;
}
