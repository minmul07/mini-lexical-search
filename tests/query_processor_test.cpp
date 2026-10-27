#include <iostream>
#include <stdexcept>
#include "mini_lexical_search/domain/query_processor.hpp"

using namespace mini_lexical_search;

void require(bool value) {
    if (!value) throw std::runtime_error("query assertion failed");
}

int main() {
    try {
        SimpleTokenizer tokenizer;
        InvertedIndex index;
        index.add("alpha", 2, 1);
        index.add("alpha", 2, 1);
        index.add("alpha", 2, 4);
        index.add("alpha", 9, 2);
        index.add("beta", 2, 4);
        index.add("beta", 2, 8);
        index.add("beta", 7, 3);
        index.add("gamma", 2, 9);
        const auto before = index.postings();
        auto search = [&](const std::string& text) {
            return QueryProcessor{}.process(parseQuery(text, tokenizer), index);
        };
        const auto single = search("ALPHA!");
        require(single.size() == 2 && single[0].documentId == 2 && single[0].score == 3);
        require(single[0].matchedLines == std::vector<std::uint32_t>({1, 4}));
        const auto both = search("alpha AND beta");
        require(both.size() == 1 && both[0].score == 5);
        require(both[0].matchedLines == std::vector<std::uint32_t>({1, 4, 8}));
        const auto either = search("alpha OR beta");
        require(either.size() == 3 && either[0].score == 5 &&
                either[1].documentId == 7 && either[2].documentId == 9);
        require(search("alpha AND beta AND gamma")[0].score == 6);
        require(search("missing OR alpha OR beta OR gamma")[0].score == 6);
        require(search("alpha AND ALPHA")[0].score == 3);
        require(search("alpha OR ALPHA")[0].score == 3);
        require(search("alpha AND missing").empty());
        require(search("missing AND alpha").empty());
        require(search("alpha OR missing").size() == 2);
        index.add("delta", 80, 1);
        require(search("alpha AND delta").empty());
        index.removeDocument(80);
        require(search("missing OR absent").empty());
        require(search("missing").empty());
        index.add("and", 42, 1);
        require(search("and")[0].documentId == 42);
        index.removeDocument(42);
        require(QueryProcessor{}.process(parseQuery("alpha", tokenizer), InvertedIndex{}).empty());
        for (const auto& invalid : {"", "  ", "!!!", "AND", "AND alpha", "alpha OR", "alpha beta",
                                    "alpha AND OR beta", "alpha AND beta OR gamma", "(alpha)",
                                    "\"alpha\"", "alpha-beta", "alpha and beta"}) {
            bool failed = false;
            try { parseQuery(invalid, tokenizer); }
            catch (const std::invalid_argument&) { failed = true; }
            require(failed);
        }
        for (const auto& [term, postings] : before) {
            const auto* actual = index.find(term);
            require(actual && actual->size() == postings.size());
            for (std::size_t i = 0; i < postings.size(); ++i) {
                require((*actual)[i].documentId == postings[i].documentId &&
                        (*actual)[i].termFrequency == postings[i].termFrequency &&
                        (*actual)[i].lines == postings[i].lines);
            }
        }
        require(index.termCount() == before.size());
        index.add("자료구조", 50, 1);
        index.add("알고리즘", 50, 3);
        require(search("자료구조 AND 알고리즘")[0].matchedLines ==
                std::vector<std::uint32_t>({1, 3}));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
