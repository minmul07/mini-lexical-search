#pragma once

#include <cstddef>
#include <functional>
#include <ios>
#include <string>
#include <vector>
#include "mini_lexical_search/domain/matching_line.hpp"
#include "mini_lexical_search/domain/document.hpp"

namespace mini_lexical_search {

// 세션에만 보관하는 원문 재개 위치. 스냅샷에는 저장하지 않는다.
struct MatchingReadPosition {
    std::streamoff offset = 0;
    std::uint64_t line = 0;
};

// nextNumber는 중복 없는 오름차순 줄 번호를, 종료 시 0을 반환한다.
// I/O 실패는 경고 문자열로 반환한다. 콜백 예외는 호출자에게 전달한다.
// 읽기를 성공한 행까지만 position과 sink에 반영한다.
std::string readMatchingLineRange(
    const Document& document, MatchingReadPosition& position,
    const std::function<std::uint32_t()>& nextNumber, std::size_t limit,
    const std::function<void(const MatchingLine&)>& sink);

// 마지막 요청 줄까지 파일을 한 번 읽는다. 입력 줄 번호는 양수이며 중복 없이 오름차순이어야 한다.
// 한도가 0이면 모든 매칭 줄을, 양수 N이면 앞 N개 매칭 줄을 읽는다.
// 읽기 실패 또는 요청 줄 부재 시 예외를 던지며, 역색인을 갱신하지 않는다.
std::vector<MatchingLine> readMatchingLines(
    const Document& document,
    const std::vector<std::uint32_t>& matchedLines,
    std::size_t maxMatchingLines = 0);

}  // namespace mini_lexical_search
