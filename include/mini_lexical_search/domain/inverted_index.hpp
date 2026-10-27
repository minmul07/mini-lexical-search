#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "mini_lexical_search/domain/document.hpp"

namespace mini_lexical_search {

// 한 문서에서 검색어가 등장한 총 횟수(termFrequency)와
// 검색어가 포함된 원본 줄 번호(1부터 시작)를 기록한다.
struct Posting {
    DocumentId documentId = 0;
    std::uint32_t termFrequency = 0;
    std::vector<std::uint32_t> lines;
};

using PostingList = std::vector<Posting>;

// 줄 번호를 포함하는 역색인: 검색어 -> posting list를 unordered_map으로 메모리에 저장한다.
// posting list는 documentId 오름차순이며, 각 posting의 줄 번호는 중복 없이 오름차순이다.
// 줄 내용과 byte offset은 저장하지 않고 줄 번호만 저장한다.
class InvertedIndex {
public:
    // 토큰의 등장 한 번을 기록한다. `line`은 1부터 시작한다.
    // 검색어가 비어 있거나 줄 번호가 0이면 std::invalid_argument 예외를 던진다.
    void add(const std::string& term, DocumentId documentId, std::uint32_t line);

    // 문서의 모든 posting을 제거하고, posting이 없어진 검색어도 제거한다.
    // 문서 삭제 또는 변경 후 재구축할 때 사용한다.
    void removeDocument(DocumentId documentId);

    // 정렬을 유지하며 `other`를 이 역색인에 병합한다.
    // 양쪽에 같은 문서가 있으면 이 역색인의 posting을 변경하기 전에
    // std::runtime_error 예외를 던진다.
    void merge(const InvertedIndex& other);

    // `term`의 posting list를 반환한다. 검색어가 없으면 nullptr를 반환한다.
    // 결과는 읽기 전용이며 역색인이 변경되기 전까지 유효하다.
    const PostingList* find(const std::string& term) const;

    const std::unordered_map<std::string, PostingList>& postings() const;
    std::size_t termCount() const;
    std::size_t postingCount() const;
    bool empty() const;

    // 파싱된 posting의 불변식을 검증한 뒤 역색인을 생성한다.
    // 검색어와 posting list는 비어 있지 않아야 하며 목록은 정렬되어야 한다.
    // 등장 횟수는 양수이고 줄 개수 이상이어야 하며, 줄 번호는 중복 없이 오름차순이어야 한다.
    static InvertedIndex fromPostings(std::unordered_map<std::string, PostingList> postings);

private:
    std::unordered_map<std::string, PostingList> postings_;
};

}  // namespace mini_lexical_search
