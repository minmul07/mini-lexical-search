#include "mini_lexical_search/presentation/tui/home_title.hpp"

#include <array>
#include <string_view>

namespace mini_lexical_search::tui {
namespace {

constexpr int kArtWidth = 53;
constexpr int kArtHeight = 6;
constexpr int kArtMargin = 2;

// ANSI Shadow 스타일의 SEARCH 아트다. 모든 줄의 표시 폭이 같아야 한다.
constexpr std::array<std::string_view, kArtHeight> kTitleArt{
    "███████╗ ███████╗  █████╗  ██████╗   ██████╗ ██╗  ██╗",
    "██╔════╝ ██╔════╝ ██╔══██╗ ██╔══██╗ ██╔════╝ ██║  ██║",
    "███████╗ █████╗   ███████║ ██████╔╝ ██║      ███████║",
    "╚════██║ ██╔══╝   ██╔══██║ ██╔══██╗ ██║      ██╔══██║",
    "███████║ ███████╗ ██║  ██║ ██║  ██║ ╚██████╗ ██║  ██║",
    "╚══════╝ ╚══════╝ ╚═╝  ╚═╝ ╚═╝  ╚═╝  ╚═════╝ ╚═╝  ╚═╝",
};

}  // namespace

HomeTitle homeTitle(int terminalWidth, int boxTop) {
    // 제목이 첫 줄에 닿으면 우측 상단 상태 라벨과 겹칠 수 있다.
    if (terminalWidth >= kArtWidth + kArtMargin && boxTop >= kArtHeight + 3) {
        HomeTitle title;
        title.top = boxTop - kArtHeight - 2;
        title.lines.reserve(kTitleArt.size());
        for (const auto line : kTitleArt) title.lines.emplace_back(line);
        return title;
    }
    if (terminalWidth >= 8 && boxTop >= 3) {
        return HomeTitle{boxTop - 3, {"SEARCH"}};
    }
    return {};
}

}  // namespace mini_lexical_search::tui
