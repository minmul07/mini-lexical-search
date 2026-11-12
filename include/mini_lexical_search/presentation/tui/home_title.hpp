#pragma once

#include <string>
#include <vector>

namespace mini_lexical_search::tui {

// 홈 입력 상자 위에 중앙 정렬할 제목이다. `top`은 화면 첫 줄부터 제목까지의 빈 줄 수다.
struct HomeTitle {
    int top = 0;
    std::vector<std::string> lines;
};

// 큰 아트 제목이 들어갈 너비·높이가 부족하면 한 줄 제목으로 줄이고,
// 그마저도 어려우면 빈 제목을 반환한다.
HomeTitle homeTitle(int terminalWidth, int boxTop);

}  // namespace mini_lexical_search::tui
