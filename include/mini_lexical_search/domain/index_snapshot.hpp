#pragma once

#include <vector>
#include "mini_lexical_search/domain/document.hpp"
#include "mini_lexical_search/domain/inverted_index.hpp"

namespace mini_lexical_search {

// 저장된 검색 색인의 메모리 상태로, 문서 테이블과 줄 번호를 포함하는 역색인으로 구성된다.
// 애플리케이션은 실행 중 하나의 snapshot을 소유한다.
struct IndexSnapshot {
    std::vector<Document> documents;
    InvertedIndex index;
};

}  // namespace mini_lexical_search
