#include <iostream>
#include <sstream>
#include <string>

#include "mini_lexical_search/version.hpp"

namespace {

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
        << "  /index             인덱싱 시작 (미구현)\n"
        << "  /search <검색어>    색인된 문서 검색 (미구현)\n"
        << "  /help              도움말 출력\n"
        << "  /exit | /quit      프로그램 종료\n";
}

// Minimal REPL skeleton. Indexing and search are not implemented yet (P0).
void runRepl(const char* program) {
    std::cout << "Mini Lexical Search started.\n"
              << "Type '/help' for commands, '/exit' to quit.\n";

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command;
        if (!(input >> command)) {
            continue;
        }
        std::string arguments;
        std::getline(input >> std::ws, arguments);

        if (command == "/search") {
            if (arguments.empty()) {
                std::cout << "Usage: /search <query>\n";
            } else {
                std::cout << "(not implemented yet) Search arrives in P0.\n";
            }
            continue;
        }
        if (command == "/index" || command == "/help" ||
            command == "/exit" || command == "/quit") {
            if (!arguments.empty()) {
                std::cout << "error: unexpected arguments for " << command << "\n";
                continue;
            }
            if (command == "/exit" || command == "/quit") {
                break;
            }
            if (command == "/help") {
                printUsage(program);
            } else {
                std::cout << "(not implemented yet) Indexing arrives in P0.\n";
            }
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
