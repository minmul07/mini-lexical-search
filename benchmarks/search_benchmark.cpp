#include "mini_lexical_search/application/search_application.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace mini_lexical_search;

// Linux RSS is read through the filesystem; no platform library is linked.
long peakKiB() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.starts_with("VmHWM:")) {
            std::istringstream value(line.substr(6));
            long result = 0;
            value >> result;
            return result;
        }
    }
    return -1;
}

template <typename Operation>
double measure(Operation operation) {
    const auto start = std::chrono::steady_clock::now();
    operation();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: search_benchmark <existing snapshot> <query>\n";
        return 1;
    }
    try {
        if (!std::filesystem::exists(argv[1])) throw std::runtime_error("snapshot does not exist");
        SearchApplication app(argv[1]);
        if (!app.ready()) throw std::runtime_error(app.loadError());
        const auto loadedPeak = peakKiB();
        SearchPage page;
        const auto firstMs = measure([&] { page = app.search(argv[2]); });
        const auto firstPeak = peakKiB();
        std::string expandable;
        for (const auto& file : page.files) {
            if (file.lines.size() < file.totalMatchingLines && file.warning.empty()) {
                expandable = file.relativePath;
                break;
            }
        }
        const auto expandMs = expandable.empty() ? -1.0 : measure([&] { app.expandFile(expandable); });
        const auto nextMs = page.hasNext ? measure([&] { app.nextPage(); }) : -1.0;
        page = {};
        app.clearSearch();
        std::size_t files = 0, rows = 0, warnings = 0;
        const auto streamMs = measure([&] {
            app.visitAllResults(argv[2], [&](const SearchResult&) { ++files; },
                [&](const MatchingLine&) { ++rows; }, [&](const std::string&) { ++warnings; });
        });
        std::cout << "first_ms,expand_ms,next_ms,stream_ms,loaded_peak_kib,first_peak_kib,final_peak_kib,files,rows,warnings\n"
                  << firstMs << ',' << expandMs << ',' << nextMs << ',' << streamMs << ','
                  << loadedPeak << ',' << firstPeak << ',' << peakKiB() << ',' << files << ',' << rows << ',' << warnings << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
