#include "mini_lexical_search/domain/inverted_index.hpp"

#include <algorithm>
#include <stdexcept>

namespace mini_lexical_search {
namespace {

bool byDocument(const Posting& posting, DocumentId documentId) {
    return posting.documentId < documentId;
}

}  // namespace

void InvertedIndex::add(const std::string& term,
                        DocumentId documentId,
                        std::uint32_t line) {
    if (term.empty()) {
        throw std::invalid_argument("token must not be empty");
    }
    if (line == 0) {
        throw std::invalid_argument("line numbers start at 1");
    }
    auto& postings = postings_[term];
    const auto posting = std::lower_bound(postings.begin(), postings.end(), documentId, byDocument);
    if (posting == postings.end() || posting->documentId != documentId) {
        postings.insert(posting, Posting{documentId, 1, {line}});
        return;
    }
    ++posting->termFrequency;
    if (posting->lines.empty() || posting->lines.back() < line) {
        posting->lines.push_back(line);
    } else if (posting->lines.back() > line) {
        const auto position =
            std::lower_bound(posting->lines.begin(), posting->lines.end(), line);
        if (position == posting->lines.end() || *position != line) {
            posting->lines.insert(position, line);
        }
    }
}

void InvertedIndex::removeDocument(DocumentId documentId) {
    for (auto entry = postings_.begin(); entry != postings_.end();) {
        auto& postings = entry->second;
        const auto posting =
            std::lower_bound(postings.begin(), postings.end(), documentId, byDocument);
        if (posting != postings.end() && posting->documentId == documentId) {
            postings.erase(posting);
        }
        if (postings.empty()) {
            entry = postings_.erase(entry);
        } else {
            ++entry;
        }
    }
}

void InvertedIndex::merge(const InvertedIndex& other) {
    // Check every term before making changes: a later conflict must not leave
    // earlier terms partially merged.
    for (const auto& [term, source] : other.postings_) {
        const auto found = postings_.find(term);
        if (found == postings_.end()) {
            continue;
        }
        const auto& target = found->second;
        std::size_t left = 0;
        std::size_t right = 0;
        while (left < target.size() && right < source.size()) {
            if (target[left].documentId < source[right].documentId) {
                ++left;
            } else if (target[left].documentId > source[right].documentId) {
                ++right;
            } else {
                throw std::runtime_error("duplicate posting for document " +
                                         std::to_string(target[left].documentId) +
                                         " in term '" + term + "'");
            }
        }
    }

    for (const auto& entry : other.postings_) {
        const auto& term = entry.first;
        const auto& source = entry.second;
        if (source.empty()) {
            continue;
        }
        const auto found = postings_.find(term);
        if (found == postings_.end()) {
            postings_.emplace(term, source);
            continue;
        }
        auto& target = found->second;
        PostingList merged;
        merged.reserve(target.size() + source.size());
        std::size_t left = 0;
        std::size_t right = 0;
        while (left < target.size() && right < source.size()) {
            const auto& posting = target[left];
            const auto& incoming = source[right];
            if (posting.documentId < incoming.documentId) {
                merged.push_back(posting);
                ++left;
            } else {
                merged.push_back(incoming);
                ++right;
            }
        }
        while (left < target.size()) {
            merged.push_back(target[left++]);
        }
        while (right < source.size()) {
            merged.push_back(source[right++]);
        }
        target = std::move(merged);
    }
}

const PostingList* InvertedIndex::find(const std::string& term) const {
    const auto entry = postings_.find(term);
    return entry == postings_.end() ? nullptr : &entry->second;
}

const std::unordered_map<std::string, PostingList>& InvertedIndex::postings() const {
    return postings_;
}

std::size_t InvertedIndex::termCount() const {
    return postings_.size();
}

std::size_t InvertedIndex::postingCount() const {
    std::size_t count = 0;
    for (const auto& entry : postings_) {
        count += entry.second.size();
    }
    return count;
}

bool InvertedIndex::empty() const {
    return postings_.empty();
}

InvertedIndex InvertedIndex::fromPostings(std::unordered_map<std::string, PostingList> postings) {
    for (const auto& entry : postings) {
        const auto& term = entry.first;
        const auto& list = entry.second;
        if (term.empty()) {
            throw std::runtime_error("invalid inverted index: empty term");
        }
        if (list.empty()) {
            throw std::runtime_error("invalid inverted index: empty posting list for '" + term + "'");
        }
        std::uint64_t previousId = 0;
        bool firstPosting = true;
        for (const auto& posting : list) {
            if (!firstPosting && posting.documentId <= previousId) {
                throw std::runtime_error("invalid inverted index: postings for '" + term +
                                         "' are not ordered by document");
            }
            previousId = posting.documentId;
            firstPosting = false;
            if (posting.termFrequency == 0) {
                throw std::runtime_error("invalid inverted index: zero term frequency for '" +
                                         term + "'");
            }
            if (posting.lines.empty()) {
                throw std::runtime_error("invalid inverted index: no lines for '" + term + "'");
            }
            if (posting.termFrequency < posting.lines.size()) {
                throw std::runtime_error("invalid inverted index: term frequency below line "
                                         "count for '" + term + "'");
            }
            std::uint32_t previousLine = 0;
            bool firstLine = true;
            for (const std::uint32_t line : posting.lines) {
                if (line == 0 || (!firstLine && line <= previousLine)) {
                    throw std::runtime_error("invalid inverted index: lines for '" + term +
                                             "' are not strictly ascending");
                }
                previousLine = line;
                firstLine = false;
            }
        }
    }
    InvertedIndex index;
    index.postings_ = std::move(postings);
    return index;
}

}  // namespace mini_lexical_search
