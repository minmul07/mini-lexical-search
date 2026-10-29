#include "mini_lexical_search/domain/top_k.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace mini_lexical_search;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool ranksAbove(const SearchCandidate& left, const SearchCandidate& right) {
    if (left.score != right.score) return left.score > right.score;
    return left.documentId < right.documentId;
}

std::string describe(const std::vector<SearchCandidate>& items) {
    std::string text;
    for (const auto& item : items) {
        text += std::to_string(item.documentId) + ":" + std::to_string(item.score) + " ";
    }
    return text;
}

void requireSame(const std::vector<SearchCandidate>& actual, const std::vector<SearchCandidate>& expected,
                 const std::string& message) {
    require(actual.size() == expected.size(), message + " size " + describe(actual));
    for (std::size_t i = 0; i < actual.size(); ++i) {
        require(actual[i].documentId == expected[i].documentId && actual[i].score == expected[i].score &&
                    actual[i].matchedLines == expected[i].matchedLines,
                message + " at " + std::to_string(i) + " actual " + describe(actual));
    }
}

std::vector<SearchCandidate> rankedPrefix(std::vector<SearchCandidate> items, std::size_t k) {
    std::sort(items.begin(), items.end(), ranksAbove);
    if (items.size() > k) items.resize(k);
    return items;
}

void checkAgainstSort(const std::vector<SearchCandidate>& items, std::size_t k) {
    requireSame(selectTopK(items, k), rankedPrefix(items, k), "k=" + std::to_string(k));
}

void checkOrders(std::vector<SearchCandidate> items, std::size_t k) {
    const auto expected = rankedPrefix(items, k);
    auto byId = [](const SearchCandidate& left, const SearchCandidate& right) {
        return left.documentId < right.documentId;
    };
    std::sort(items.begin(), items.end(), byId);
    do {
        requireSame(selectTopK(items, k), expected, "order k=" + std::to_string(k));
    } while (std::next_permutation(items.begin(), items.end(), byId));
}

}  // namespace

int main() {
    try {
        require(selectTopK({}, 5).empty(), "empty candidates");
        require(selectTopK({{1, 3.0, {1}}}, 0).empty(), "k zero");
        require(selectTopK({}, 0).empty(), "empty and k zero");

        const std::vector<SearchCandidate> boundary{
            {4, 1.0, {9}},
            {3, 5.0, {4}},
            {1, 10.0, {8, 9}},
            {2, 5.0, {2, 3}},
        };
        checkOrders(boundary, 0);
        checkOrders(boundary, 1);
        checkOrders(boundary, 2);
        checkOrders(boundary, 3);
        checkOrders(boundary, 4);
        checkOrders(boundary, 5);
        const auto topTwo = selectTopK(boundary, 2);
        require(topTwo.size() == 2 && topTwo[0].documentId == 1 && topTwo[0].matchedLines ==
                                           std::vector<std::uint32_t>{8, 9} &&
                    topTwo[1].documentId == 2 && topTwo[1].score == 5.0 &&
                    topTwo[1].matchedLines == std::vector<std::uint32_t>{2, 3},
                "boundary tie kept the larger id");

        const std::vector<SearchCandidate> sameScore{
            {5, 2.0, {1}},
            {1, 2.0, {2}},
            {3, 2.0, {3}},
            {2, 2.0, {}},
        };
        checkOrders(sameScore, 1);
        checkOrders(sameScore, 3);
        checkOrders(sameScore, 4);
        const auto tied = selectTopK(sameScore, 4);
        require(tied[3].documentId == 5 && tied[1].matchedLines.empty() && tied[1].documentId == 2,
                "equal scores did not follow document id");

        const std::vector<SearchCandidate> distinct{
            {1, 1.0, {1}},
            {2, 4.0, {2}},
            {3, 3.0, {3}},
            {4, 2.0, {4}},
        };
        checkAgainstSort(distinct, 1);
        checkAgainstSort(distinct, 3);
        auto reversed = distinct;
        std::reverse(reversed.begin(), reversed.end());
        requireSame(selectTopK(distinct, 3), selectTopK(reversed, 3), "input order changed ranking");

        std::vector<SearchCandidate> many;
        for (DocumentId id = 0; id < 50; ++id) {
            many.push_back({id, static_cast<double>((id * 17) % 11), {id + 1}});
        }
        for (const auto k : {std::size_t{0}, std::size_t{1}, std::size_t{5}, std::size_t{10},
                             std::size_t{11}, std::size_t{50}, std::size_t{51}}) {
            checkAgainstSort(many, k);
        }
        checkAgainstSort(many, std::numeric_limits<std::size_t>::max());
        require(selectTopK(many, 1)[0].score == 10.0, "single result was not the best score");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
