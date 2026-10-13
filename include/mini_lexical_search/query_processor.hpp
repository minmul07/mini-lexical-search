#pragma once

#include "mini_lexical_search/inverted_index.hpp"
#include "mini_lexical_search/tokenizer.hpp"

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

// Uppercase standalone AND/OR are operators. Mixed operators, implicit
// operators, parentheses and quotes are unsupported. Throws invalid_argument.
ParsedQuery parseQuery(const std::string& text, Tokenizer& tokenizer);

class QueryProcessor {
public:
    // Returns all matches in document ID order without modifying the index.
    // Terms are normalized by parseQuery; repeated terms contribute only once.
    std::vector<SearchCandidate> process(const ParsedQuery& query,
                                         const InvertedIndex& index) const;
};

}  // namespace mini_lexical_search
