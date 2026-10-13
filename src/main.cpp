#include <iostream>
#include <string>

#include "mini_lexical_search/presentation/cli/cli.hpp"
#include "mini_lexical_search/presentation/tui/tui.hpp"
#include "mini_lexical_search/domain/version.hpp"

int main(int argc, char* argv[]) {
    bool start = false;
    bool noTui = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            mini_lexical_search::cli::printUsage(argv[0]);
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            std::cout << "mini_lexical_search 버전 " << mini_lexical_search::versionString() << "\n";
            return 0;
        }
        if (arg == "--no-tui") {
            if (noTui) {
                std::cerr << "error: unexpected argument: " << arg << "\n";
                mini_lexical_search::cli::printUsage(argv[0]);
                return 1;
            }
            noTui = true;
            continue;
        }
        if (arg != "start" || start) {
            std::cerr << "error: unexpected argument: " << arg << "\n";
            mini_lexical_search::cli::printUsage(argv[0]);
            return 1;
        }
        start = true;
    }

    if (!start) {
        mini_lexical_search::cli::printUsage(argv[0]);
        return 1;
    }

    if (noTui) {
        mini_lexical_search::SearchApplication application(".data/index.json");
        mini_lexical_search::cli::runRepl(argv[0], application);
        return 0;
    }

    mini_lexical_search::tui::run();
    return 0;
}
