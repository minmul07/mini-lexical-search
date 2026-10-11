#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <unordered_map>

#include "mini_lexical_search/document.hpp"
#include "mini_lexical_search/indexer.hpp"
#include "mini_lexical_search/matching_lines.hpp"
#include "mini_lexical_search/query_processor.hpp"
#include "mini_lexical_search/version.hpp"

namespace {

constexpr const char* kIndexPath = ".data/index.json";
constexpr std::size_t kMaxMatchingLines = 0;  // Zero means unlimited per document.

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
            } else if (!indexLoaded) {
                std::cout << "error: index snapshot is not loaded; fix or remove " << kIndexPath << "\n";
            } else {
                try {
                    mini_lexical_search::SimpleTokenizer tokenizer;
                    const auto query = mini_lexical_search::parseQuery(arguments, tokenizer);
                    const auto candidates = mini_lexical_search::QueryProcessor{}.process(query, snapshot.index);
                    if (candidates.empty()) std::cout << "No matches.\n";
                    std::unordered_map<mini_lexical_search::DocumentId, const mini_lexical_search::Document*> documents;
                    for (const auto& document : snapshot.documents) documents.emplace(document.id, &document);
                    for (std::size_t i = 0; i < candidates.size(); ++i) {
                        const auto& candidate = candidates[i];
                        const auto& document = *documents.at(candidate.documentId);
                        std::cout << i + 1 << ". " << document.title << " (" << document.path
                                  << ")    score: " << candidate.score << "\n";
                        try {
                            const auto lines = mini_lexical_search::readMatchingLines(
                                document, candidate.matchedLines, kMaxMatchingLines);
                            for (const auto& match : lines) {
                                std::cout << "   " << match.number << ": " << match.text << "\n";
                            }
                            if (lines.size() < candidate.matchedLines.size()) {
                                std::cout << "   + " << candidate.matchedLines.size() - lines.size()
                                          << " more matches\n";
                            }
                        } catch (const std::exception& error) {
                            std::cout << "   warning: " << error.what() << "\n";
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
