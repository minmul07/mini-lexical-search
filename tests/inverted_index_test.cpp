#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mini_lexical_search/domain/inverted_index.hpp"

using mini_lexical_search::DocumentId;
using mini_lexical_search::InvertedIndex;
using mini_lexical_search::PostingList;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Operation>
void requireFailure(Operation operation, const std::string& message) {
    bool failed = false;
    try {
        operation();
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed, message);
}

void requireSameIndex(const InvertedIndex& actual, const InvertedIndex& expected,
                      const std::string& message) {
    require(actual.termCount() == expected.termCount(), message);
    for (const auto& [term, postings] : expected.postings()) {
        const auto* found = actual.find(term);
        require(found != nullptr && found->size() == postings.size(), message);
        for (std::size_t i = 0; i < postings.size(); ++i) {
            require((*found)[i].documentId == postings[i].documentId &&
                        (*found)[i].termFrequency == postings[i].termFrequency &&
                        (*found)[i].lines == postings[i].lines,
                    message);
        }
    }
}

void testMergeConflict(bool extendExisting) {
    InvertedIndex source;
    source.add("alpha", 3, 2);
    source.add("beta", 3, 4);
    const auto sourceBefore = source;

    // Derive the conflict from actual iteration order, not hash implementation details.
    auto entry = source.postings().begin();
    const auto firstTerm = entry->first;
    const auto conflictTerm = (++entry)->first;
    InvertedIndex target;
    target.add("untouched", 9, 1);
    target.add(conflictTerm, 1, 1);
    target.add(conflictTerm, 3, 5);
    target.add(conflictTerm, 3, 5);
    target.add(conflictTerm, 3, 7);
    if (extendExisting) {
        target.add(firstTerm, 1, 3);
    }
    const auto before = target;
    bool failed = false;
    try {
        target.merge(source);
    } catch (const std::runtime_error& error) {
        failed = true;
        require(std::string(error.what()) ==
                    "duplicate posting for document 3 in term '" + conflictTerm + "'",
                "merge must preserve duplicate error details");
    }
    require(failed, "late duplicate posting must throw runtime_error");
    requireSameIndex(target, before, extendExisting
        ? "failed merge must not extend an earlier term"
        : "failed merge must not insert an earlier term");
    requireSameIndex(source, sourceBefore, "failed merge must preserve source");
}

int main() {
    try {
        testMergeConflict(false);
        testMergeConflict(true);

        InvertedIndex target;
        target.add("shared", 2, 3);
        target.add("shared", 4, 5);
        InvertedIndex source;
        source.add("shared", 1, 2);
        source.add("shared", 3, 4);
        source.add("shared", 5, 6);
        source.add("distinct", 2, 1);
        const auto sourceBefore = source;
        target.merge(source);
        InvertedIndex expected = source;
        expected.add("shared", 2, 3);
        expected.add("shared", 4, 5);
        requireSameIndex(target, expected, "merge must keep interleaved postings sorted");
        requireSameIndex(source, sourceBefore, "successful merge must preserve source");
        InvertedIndex empty;
        target.merge(empty);
        requireSameIndex(target, expected, "empty source must leave target unchanged");
        empty.merge(target);
        requireSameIndex(empty, expected, "empty target must receive all postings");
        requireFailure([&] { target.merge(target); }, "nonempty self merge must fail");
        requireSameIndex(target, expected, "failed self merge must preserve all postings");
        InvertedIndex emptySelf;
        emptySelf.merge(emptySelf);
        require(emptySelf.empty(), "empty self merge must remain empty");

        InvertedIndex index;
        require(index.empty() && index.termCount() == 0 && index.postingCount() == 0,
                "new index must be empty");
        require(index.find("missing") == nullptr, "missing term must return nullptr");

        // A token repeated on one line counts once for the line but every time
        // for the term frequency.
        index.add("자료구조", 0, 1);
        index.add("자료구조", 0, 1);
        index.add("자료구조", 0, 2);
        index.add("자료구조", 0, 2);
        index.add("자료구조", 0, 2);
        require(index.termCount() == 1 && index.postingCount() == 1, "posting counters failed");
        const auto* postings = index.find("자료구조");
        require(postings != nullptr && postings->size() == 1, "posting list size failed");
        require((*postings)[0].termFrequency == 5,
                "termFrequency must count every occurrence");
        require((*postings)[0].lines == std::vector<std::uint32_t>{1, 2},
                "lines must be unique and ascending");

        // Out-of-order lines stay sorted and unique.
        index.add("term", 2, 7);
        index.add("term", 2, 3);
        index.add("term", 2, 3);
        index.add("term", 2, 5);
        const auto* ordered = index.find("term");
        require((*ordered)[0].lines == std::vector<std::uint32_t>{3, 5, 7},
                "lines must stay sorted");

        // Posting lists stay ordered by document and reuse existing postings.
        index.add("term", 0, 1);
        index.add("term", 1, 2);
        index.add("term", 0, 9);
        const auto* shared = index.find("term");
        require(shared->size() == 3, "postings per document failed");
        require((*shared)[0].documentId == 0 && (*shared)[1].documentId == 1 &&
                    (*shared)[2].documentId == 2,
                "posting lists must be ordered by document");
        require((*shared)[0].termFrequency == 2, "existing posting must be reused");

        requireFailure([&] { index.add("", 0, 1); }, "empty term must be rejected");
        requireFailure([&] { index.add("term", 0, 0); }, "line 0 must be rejected");

        // Merge keeps both sides and refuses overlapping documents.
        InvertedIndex other;
        other.add("term", 5, 4);
        other.add("new", 5, 1);
        index.merge(other);
        require(index.find("new") != nullptr, "merge must add new terms");
        require(index.find("term")->size() == 4, "merge must extend existing terms");
        require((*index.find("term"))[3].documentId == 5, "merged posting order failed");
        requireFailure([&] { index.merge(other); }, "overlapping merge must fail");
        require(index.find("term")->size() == 4, "failed merge must not change the index");

        // Removing a document drops only its postings.
        index.removeDocument(0);
        require(index.find("자료구조") == nullptr, "empty terms must disappear");
        require(index.find("term")->size() == 3, "removing a document keeps other postings");
        index.removeDocument(1);
        index.removeDocument(2);
        require(index.find("term")->size() == 1 &&
                    (*index.find("term"))[0].documentId == 5,
                "removing documents must keep the merged posting");
        index.removeDocument(5);
        require(index.empty(), "removing every posting must empty the index");

        // fromPostings validates the persisted invariants.
        using Postings = std::unordered_map<std::string, PostingList>;
        const auto valid = InvertedIndex::fromPostings(
            Postings{{"a", {{0, 2, {1, 3}}}}});
        require(valid.find("a") != nullptr && valid.postingCount() == 1,
                "valid postings must load");
        for (const auto& entry : std::vector<std::pair<std::string, Postings>>{
                 {"empty term", {{"", {{0, 1, {1}}}}}},
                 {"empty posting list", {{"a", {}}}},
                 {"zero term frequency", {{"a", {{0, 0, {}}}}}},
                 {"empty lines", {{"a", {{0, 1, {}}}}}},
                 {"term frequency below line count", {{"a", {{0, 1, {1, 2}}}}}},
                 {"line zero", {{"a", {{0, 1, {0}}}}}},
                 {"unsorted lines", {{"a", {{0, 2, {2, 1}}}}}},
                 {"duplicate lines", {{"a", {{0, 2, {1, 1}}}}}},
                 {"unsorted documents", {{"a", {{1, 1, {1}}, {0, 1, {1}}}}}},
                 {"duplicate documents", {{"a", {{0, 2, {1}}, {0, 2, {1}}}}}},
             }) {
            requireFailure([&] { InvertedIndex::fromPostings(entry.second); },
                           entry.first + " must be rejected");
        }

        std::cout << "inverted_index_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
