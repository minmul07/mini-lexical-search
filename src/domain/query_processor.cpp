#include "mini_lexical_search/domain/query_processor.hpp"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace mini_lexical_search {
namespace {

SearchCandidate candidate(const Posting& posting) {
    return {posting.documentId, static_cast<double>(posting.termFrequency), posting.lines};
}

std::vector<SearchCandidate> mergeCandidates(const std::vector<SearchCandidate>& left,
                                            const PostingList& right, bool intersect) {
    std::vector<SearchCandidate> result;
    std::size_t i = 0, j = 0;
    while (i < left.size() && j < right.size()) {
        if (left[i].documentId < right[j].documentId) {
            if (!intersect) result.push_back(left[i]);
            ++i;
        } else if (left[i].documentId > right[j].documentId) {
            if (!intersect) result.push_back(candidate(right[j]));
            ++j;
        } else {
            SearchCandidate combined{left[i].documentId,
                                     left[i].score + static_cast<double>(right[j].termFrequency), {}};
            std::set_union(left[i].matchedLines.begin(), left[i].matchedLines.end(),
                           right[j].lines.begin(), right[j].lines.end(),
                           std::back_inserter(combined.matchedLines));
            result.push_back(std::move(combined));
            ++i;
            ++j;
        }
    }
    if (!intersect) {
        while (i < left.size()) result.push_back(left[i++]);
        while (j < right.size()) result.push_back(candidate(right[j++]));
    }
    return result;
}

}  // namespace

ParsedQuery parseQuery(const std::string& text, Tokenizer& tokenizer) {
    if (text.find_first_of("()\"'") != std::string::npos) {
        throw std::invalid_argument("parentheses and quotes are not supported");
    }
    ParsedQuery query;
    std::unordered_set<std::string> seen;
    std::istringstream input(text);
    std::string word;
    bool expectTerm = true;
    while (input >> word) {
        const bool isOperator = word == "AND" || word == "OR";
        if (expectTerm) {
            if (isOperator) throw std::invalid_argument("expected a keyword");
            auto tokens = tokenizer.tokenize(word);
            if (tokens.size() != 1) {
                throw std::invalid_argument("each keyword must normalize to exactly one token");
            }
            if (seen.insert(tokens.front()).second) query.terms.push_back(tokens.front());
        } else {
            if (!isOperator) throw std::invalid_argument("expected AND or OR between keywords");
            const auto mode = word == "AND" ? QueryMode::And : QueryMode::Or;
            if (query.mode != QueryMode::Keyword && query.mode != mode) {
                throw std::invalid_argument("mixed AND/OR operators are not supported");
            }
            query.mode = mode;
        }
        expectTerm = !expectTerm;
    }
    if (expectTerm) throw std::invalid_argument("query is empty or ends with an operator");
    return query;
}

std::vector<SearchCandidate> QueryProcessor::process(const ParsedQuery& query,
                                                    const InvertedIndex& index) const {
    if (query.terms.empty() ||
        (query.mode == QueryMode::Keyword && query.terms.size() != 1) ||
        (query.mode != QueryMode::Keyword && query.mode != QueryMode::And && query.mode != QueryMode::Or)) {
        throw std::invalid_argument("invalid parsed query");
    }
    std::vector<SearchCandidate> result;
    std::unordered_set<std::string> seen;
    bool first = true;
    for (const auto& term : query.terms) {
        if (term.empty()) throw std::invalid_argument("empty query term");
        if (!seen.insert(term).second) continue;
        const auto* postings = index.find(term);
        if (!postings) {
            if (query.mode != QueryMode::Or) return {};
            continue;
        }
        if (first) {
            for (const auto& posting : *postings) result.push_back(candidate(posting));
            first = false;
        } else {
            result = mergeCandidates(result, *postings, query.mode == QueryMode::And);
        }
        if (query.mode == QueryMode::And && result.empty()) return {};
    }
    return result;
}

}  // namespace mini_lexical_search
