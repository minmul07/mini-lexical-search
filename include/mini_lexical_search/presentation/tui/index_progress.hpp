#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

#include "ftxui.hpp"
#include "mini_lexical_search/application/search_application.hpp"

namespace mini_lexical_search::tui {

// worker 스레드가 갱신하고 UI 스레드가 복사본을 읽는다.
struct IndexDisplay {
    bool active = false;
    bool finished = false;
    bool failed = false;
    float target = 0.0f;
    std::string error;
    std::vector<std::string> lines;
};

// 검색창은 홈 화면의 같은 위치에 두고, 상세 로그만 그 아래에 배치하기 위한 계산값이다.
struct HomeGeometry {
    int fieldWidth = 1;
    int fieldLeft = 0;
    int boxTop = 0;
    int boxHeight = 3;
    int detailTop = 0;
    int detailHeight = 0;
};

int homeFieldWidth(int terminalWidth);
HomeGeometry homeGeometry(int terminalWidth, int terminalHeight, int inputHeight);

// 검색창 내부를 왼쪽부터 `progress` 비율까지 연한 회색으로 채운다.
// `opacity`가 1보다 작으면 채움 색을 어둡게 보간하여 fade-out한다.
ftxui::Element progressFill(ftxui::Element child, float progress, float opacity = 1.0F);

class IndexProgressModel {
public:
    void begin();
    void clear();
    void apply(const IndexProgress& event);
    void finish(const IndexUpdate& result);
    void fail(std::string message);
    IndexDisplay display() const;

private:
    void replaceLine(const std::string& prefix, std::string line);

    mutable std::mutex mutex_;
    bool active_ = false;
    bool finished_ = false;
    bool failed_ = false;
    float target_ = 0.0f;
    std::string error_;
    std::vector<std::string> lines_;
};

}  // namespace mini_lexical_search::tui
