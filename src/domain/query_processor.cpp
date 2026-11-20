#include "mini_lexical_search/domain/query_processor.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

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

    // 공백으로 구분된 키워드는 OR, '&'로 구분된 키워드는 AND로 해석한다.
    // 두 구분자를 한 질의에서 섞는 것은 지원하지 않는다.
    enum class PartKind { Keyword, And };
    struct Part {
        PartKind kind;
        std::string text;
    };
    const auto isSpace = [](char ch) {
        return std::isspace(static_cast<unsigned char>(ch)) != 0;
    };

    std::vector<Part> parts;
    for (std::size_t index = 0; index < text.size();) {
        if (isSpace(text[index])) {
            ++index;
        } else if (text[index] == '&') {
            parts.push_back({PartKind::And, {}});
            ++index;
        } else {
            const auto begin = index;
            while (index < text.size() && text[index] != '&' && !isSpace(text[index])) ++index;
            parts.push_back({PartKind::Keyword, text.substr(begin, index - begin)});
        }
    }

    if (parts.empty()) throw std::invalid_argument("query is empty or ends with an operator");
    if (parts.front().kind == PartKind::And) {
        throw std::invalid_argument("query is empty or starts with an operator");
    }
    for (std::size_t i = 1; i < parts.size(); ++i) {
        if (parts[i].kind == PartKind::And && parts[i - 1].kind == PartKind::And) {
            throw std::invalid_argument("expected a keyword between & operators");
        }
    }
    if (parts.back().kind == PartKind::And) {
        throw std::invalid_argument("query is empty or ends with an operator");
    }

    std::size_t keywordCount = 0;
    bool hasAnd = false;
    bool hasImplicitOr = false;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (parts[i].kind == PartKind::And) {
            hasAnd = true;
        } else {
            ++keywordCount;
            if (i > 0 && parts[i - 1].kind == PartKind::Keyword) hasImplicitOr = true;
        }
    }
    if (hasAnd && hasImplicitOr) {
        throw std::invalid_argument("mixed '&' and whitespace-separated OR are not supported");
    }

    ParsedQuery query;
    query.mode = hasAnd ? QueryMode::And
                        : (keywordCount > 1 ? QueryMode::Or : QueryMode::Keyword);
    std::unordered_set<std::string> seen;
    for (const auto& part : parts) {
        if (part.kind == PartKind::And) continue;
        auto tokens = tokenizer.tokenize(part.text);
        if (tokens.size() != 1) {
            throw std::invalid_argument("each keyword must normalize to exactly one token");
        }
        if (seen.insert(tokens.front()).second) query.terms.push_back(tokens.front());
    }
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

std::vector<DocumentId> QueryProcessor::matchingDocuments(const ParsedQuery& query,
                                                           const InvertedIndex& index) const {
    if (query.terms.empty() ||
        (query.mode == QueryMode::Keyword && query.terms.size() != 1) ||
        (query.mode != QueryMode::Keyword && query.mode != QueryMode::And && query.mode != QueryMode::Or)) {
        throw std::invalid_argument("invalid parsed query");
    }
    std::vector<DocumentId> result;
    bool first = true;
    for (const auto& term : query.terms) {
        if (term.empty()) throw std::invalid_argument("empty query term");
        const auto* list = index.find(term);
        if (!list) {
            if (query.mode != QueryMode::Or) return {};
            continue;
        }
        std::vector<DocumentId> right;
        right.reserve(list->size());
        for (const auto& posting : *list) right.push_back(posting.documentId);
        if (first) {
            result = std::move(right);
            first = false;
        } else {
            std::vector<DocumentId> merged;
            if (query.mode == QueryMode::And) {
                std::set_intersection(result.begin(), result.end(), right.begin(), right.end(),
                                      std::back_inserter(merged));
            } else {
                std::set_union(result.begin(), result.end(), right.begin(), right.end(),
                               std::back_inserter(merged));
            }
            result = std::move(merged);
        }
        if (query.mode == QueryMode::And && result.empty()) return {};
    }
    return result;
}

}  // namespace mini_lexical_search
