#include "mini_lexical_search/domain/top_k.hpp"

#include <algorithm>
#include <queue>
#include <utility>
#include <vector>

namespace mini_lexical_search {
namespace {

bool ranksAbove(const SearchCandidate& left, const SearchCandidate& right) {
    if (left.score != right.score) return left.score > right.score;
    return left.documentId < right.documentId;
}

// priority_queue는 비교에서 앞서는 원소를 아래로 보낸다.
// ranksAbove가 참이면 왼쪽이 더 높은 순위이므로 top은 가장 낮은 순위다.
struct RanksAbove {
    const std::vector<SearchCandidate>* candidates = nullptr;

    bool operator()(std::size_t left, std::size_t right) const {
        return ranksAbove((*candidates)[left], (*candidates)[right]);
    }
};

}  // namespace

std::vector<SearchCandidate> selectTopK(std::vector<SearchCandidate> candidates, std::size_t k) {
    if (k == 0 || candidates.empty()) return {};

    const RanksAbove order{&candidates};
    const auto limit = std::min(k, candidates.size());
    std::priority_queue<std::size_t, std::vector<std::size_t>, RanksAbove> lowestRank(order);
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        if (lowestRank.size() < limit) {
            lowestRank.push(index);
        } else if (order(index, lowestRank.top())) {
            lowestRank.pop();
            lowestRank.push(index);
        }
    }

    std::vector<std::size_t> selected;
    selected.reserve(lowestRank.size());
    while (!lowestRank.empty()) {
        selected.push_back(lowestRank.top());
        lowestRank.pop();
    }
    std::sort(selected.begin(), selected.end(), order);

    std::vector<SearchCandidate> result;
    result.reserve(selected.size());
    for (const auto index : selected) result.push_back(std::move(candidates[index]));
    return result;
}

}  // namespace mini_lexical_search
