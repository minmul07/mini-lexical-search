#include "mini_lexical_search/application/log.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace mini_lexical_search {
namespace {

std::string localTimestamp() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    if (localtime_r(&now, &local) == nullptr) {
        return "0000-00-00 00:00:00";
    }
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

}  // namespace

std::string formatElapsedMilliseconds(std::int64_t microseconds) {
    std::ostringstream out;
    out << (microseconds / 1000) << '.' << std::setw(3) << std::setfill('0')
        << (microseconds % 1000);
    return out.str();
}

void appendLog(std::string_view message) {
    try {
        std::string line = localTimestamp();
        line.push_back(' ');
        for (const char ch : message) {
            line.push_back(ch == '\n' || ch == '\r' ? ' ' : ch);
        }
        line.push_back('\n');

        const std::filesystem::path path = ".data/log.log";
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::app);
        if (!output) {
            return;
        }
        output.write(line.data(), static_cast<std::streamsize>(line.size()));
    } catch (...) {
    }
}

}  // namespace mini_lexical_search
