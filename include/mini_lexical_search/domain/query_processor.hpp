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

// 독립된 대문자 AND/OR를 연산자로 인식한다. 연산자 혼합, 암묵적 연산자,
// 괄호와 따옴표는 지원하지 않는다. 오류 시 invalid_argument 예외를 던진다.
ParsedQuery parseQuery(const std::string& text, Tokenizer& tokenizer);

class QueryProcessor {
public:
    // 역색인을 변경하지 않고 모든 매칭 결과를 문서 ID순으로 반환한다.
    // 검색어는 parseQuery로 정규화하며, 반복된 검색어는 한 번만 계산에 반영한다.
    std::vector<SearchCandidate> process(const ParsedQuery& query,
                                         const InvertedIndex& index) const;
};

}  // namespace mini_lexical_search
