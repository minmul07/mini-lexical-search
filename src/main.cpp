#include <filesystem>
#include <iostream>
#include <string>

#include "mini_search/version.hpp"

namespace {

void printUsage(const char* program) {
    std::cout
        << "Mini Search Engine " << mini_search::versionString() << "\n\n"
        << "사용법:\n"
        << "  " << program << " [옵션] <문서_디렉터리>\n\n"
        << "옵션:\n"
        << "  -h, --help     도움말 출력\n"
        << "  -v, --version  버전 정보 출력\n\n"
        << "대화형 명령:\n"
        << "  search <검색어>  색인된 문서 검색\n"
        << "  exit | quit      프로그램 종료\n";
}

// Minimal REPL skeleton. Indexing and search are not implemented yet (P0).
void runRepl(const std::filesystem::path& documentsDirectory) {
    std::cout << "Documents directory: " << documentsDirectory.string() << "\n"
              << "Type 'search <query>' to search, 'exit' to quit.\n";

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        if (line == "exit" || line == "quit") {
            break;
        }
        if (line.empty()) {
            continue;
        }
        if (line.rfind("search", 0) == 0) {
            std::cout << "(not implemented yet) Indexing and search arrive in P0.\n";
            continue;
        }
        std::cout << "Unknown command: " << line << "\n";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string documentsDirectory;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            std::cout << "mini_search 버전 " << mini_search::versionString() << "\n";
            return 0;
        }
        if (!documentsDirectory.empty()) {
            std::cerr << "error: unexpected argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
        documentsDirectory = arg;
    }

    if (documentsDirectory.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    const std::filesystem::path directory(documentsDirectory);
    if (!std::filesystem::is_directory(directory)) {
        std::cerr << "error: not a directory: " << documentsDirectory << "\n";
        return 1;
    }

    runRepl(directory);
    return 0;
}
