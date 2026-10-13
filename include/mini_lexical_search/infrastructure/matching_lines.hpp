#pragma once

#include <cstddef>
#include <vector>
#include "mini_lexical_search/domain/matching_line.hpp"
#include "mini_lexical_search/domain/document.hpp"

namespace mini_lexical_search {

// 마지막 요청 줄까지 파일을 한 번 읽는다. 입력 줄 번호는 양수이며 중복 없이 오름차순이어야 한다.
// 한도가 0이면 모든 매칭 줄을, 양수 N이면 앞 N개 매칭 줄을 읽는다.
// 읽기 실패 또는 요청 줄 부재 시 예외를 던지며, 역색인을 갱신하지 않는다.
std::vector<MatchingLine> readMatchingLines(
    const Document& document,
    const std::vector<std::uint32_t>& matchedLines,
    std::size_t maxMatchingLines = 0);

}  // namespace mini_lexical_search
