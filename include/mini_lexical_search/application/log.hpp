#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace mini_lexical_search {

// 마이크로초를 소수점 셋째 자리 밀리초로 만든다. 1234는 "1.234"이다.
std::string formatElapsedMilliseconds(std::int64_t microseconds);

// 현재 작업 디렉터리의 `.data/log.log`에 이벤트 한 줄을 추가한다.
// 줄 앞에 로컬 시각을 붙이고, 내용의 개행은 공백으로 바꾼다.
// 디렉터리가 없으면 만든다. 기록에 실패해도 예외를 던지지 않는다.
void appendLog(std::string_view message);

}  // namespace mini_lexical_search
