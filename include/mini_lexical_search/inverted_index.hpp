#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "mini_lexical_search/document.hpp"

namespace mini_lexical_search {

// Where one term occurred inside one document: how many times it occurred in
// total (termFrequency) and which original 1-based lines contain it.
struct Posting {
    DocumentId documentId = 0;
    std::uint32_t termFrequency = 0;
    std::vector<std::uint32_t> lines;
};

using PostingList = std::vector<Posting>;

// Line-aware Inverted Index: term -> Posting List, kept in memory as an
// unordered_map. Posting lists are ordered by documentId, and each posting
// keeps unique, ascending line numbers. Line text and byte offsets are not
// stored; only the numbers are.
class InvertedIndex {
public:
    // Records one token occurrence. `line` is 1-based. Throws
    // std::invalid_argument for an empty term or a zero line number.
    void add(const std::string& term, DocumentId documentId, std::uint32_t line);

    // Drops every posting of the document and removes terms left with no
    // postings. Used when a document is deleted or rebuilt after a change.
    void removeDocument(DocumentId documentId);

    // Merges `other` into this index while keeping both sides sorted. A
    // document present in both throws std::runtime_error before any posting of
    // this index is changed.
    void merge(const InvertedIndex& other);

    // Returns the postings of `term`, or nullptr when the term is absent. The
    // result is read-only and stays valid until the index is modified.
    const PostingList* find(const std::string& term) const;

    const std::unordered_map<std::string, PostingList>& postings() const;
    std::size_t termCount() const;
    std::size_t postingCount() const;
    bool empty() const;

    // Builds an index from parsed postings after validating the invariants:
    // non-empty terms, non-empty ordered posting lists, positive term
    // frequencies not below the line count, and unique ascending lines.
    static InvertedIndex fromPostings(std::unordered_map<std::string, PostingList> postings);

private:
    std::unordered_map<std::string, PostingList> postings_;
};

}  // namespace mini_lexical_search
