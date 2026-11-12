#pragma once

#include <string>
#include <string_view>

namespace mini_lexical_search::tui {

// 선택 텍스트를 터미널 클립보드로 보내는 OSC 52 시퀀스를 만든다.
std::string osc52ClipboardSequence(std::string_view text);
// OSC 52 시퀀스를 표준 출력에 쓴다. 터미널의 지원 여부는 확인하지 않는다.
bool copyToClipboard(std::string_view text);

} // namespace mini_lexical_search::tui
