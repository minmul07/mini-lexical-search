#pragma once

#include "mini_lexical_search/domain/inverted_index.hpp"
#include "mini_lexical_search/domain/tokenizer.hpp"

namespace mini_lexical_search {

enum class QueryMode { Keyword, And, Or };

struct ParsedQuery {
    QueryMode mode = QueryMode::Keyword;
    std::vector<std::string> terms;
};

struct SearchCandidate {
    DocumentId documentId;
    double score;
    std::vector<std::uint32_t> matchedLines;
};

// 공백으로 구분된 키워드는 OR, '&'로 구분된 키워드는 AND로 해석한다.
// 한 질의에서 두 구분자를 섞거나 괄호·따옴표를 쓰면 invalid_argument를 던진다.
// AND/OR는 연산자가 아니므로 일반 키워드로 검색된다.
ParsedQuery parseQuery(const std::string& text, Tokenizer& tokenizer);

class QueryProcessor {
public:
    // 역색인을 변경하지 않고 모든 매칭 결과를 문서 ID순으로 반환한다.
    // 검색어는 parseQuery로 정규화하며, 반복된 검색어는 한 번만 계산에 반영한다.
    // 매칭 줄을 복사하지 않고 일치 문서 ID만 오름차순으로 계산한다.
    std::vector<DocumentId> matchingDocuments(const ParsedQuery& query,
                                             const InvertedIndex& index) const;
    std::vector<SearchCandidate> process(const ParsedQuery& query,
                                         const InvertedIndex& index) const;
};

}  // namespace mini_lexical_search
