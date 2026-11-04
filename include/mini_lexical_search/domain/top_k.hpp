#pragma once

#include <cstddef>
#include <vector>

#include "mini_lexical_search/domain/query_processor.hpp"

namespace mini_lexical_search {

// 점수 내림차순, 동점은 문서 ID 오름차순으로 상위 k개를 고른다.
// k가 0이면 빈 결과를 반환한다. 후보가 k보다 적으면 모두 반환한다.
// 크기 min(k, 후보 수)의 min heap으로 고르며, 후보 수 M에 대해 선택은 O(M log K),
// 결과 정렬은 O(K log K), heap의 추가 공간은 O(K)이다.
// 호출자가 후보를 넘길 수 있도록 값으로 받는다. 후보는 이동하고 역색인은 바꾸지 않는다.
std::vector<SearchCandidate> selectTopK(std::vector<SearchCandidate> candidates, std::size_t k);

}  // namespace mini_lexical_search
