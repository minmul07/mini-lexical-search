#pragma once

#include "mini_lexical_search/application/documents.hpp"
#include "mini_lexical_search/domain/index_snapshot.hpp"

namespace mini_lexical_search {

struct IndexUpdate {
    IndexStats metadata;
    TokenizeStats tokens;
};

// 작업 대상 중 추가·변경된 문서의 posting을 재구축하고 오래된 posting을 제거한다.
// 먼저 임시 역색인에 토큰을 모으므로, 문서 읽기가 실패하면 `index`를 변경하지 않는다.
TokenizeStats applyIndexWork(IndexWork& work,
                             InvertedIndex& index,
                             Tokenizer& tokenizer);

// 새 문서 테이블과 역색인을 저장한 뒤 `snapshot`을 교체한다.
// 저장 실패 시 메모리 snapshot을 마지막 저장 상태로 복구하고 예외를 다시 던진다.
// 저장된 상태가 없으면 빈 snapshot으로 복구한다.
void commitIndexSnapshot(std::vector<Document> documents,
                         IndexSnapshot& snapshot,
                         const std::filesystem::path& output);

// 비교·토큰화·posting 재구축을 수행하고 문서 테이블이 변경되었으면 저장한다.
// `/index`에 표시하는 집계 값을 반환한다.
IndexUpdate updateIndexSnapshot(const std::filesystem::path& directory,
                                IndexSnapshot& snapshot,
                                const std::filesystem::path& output,
                                Tokenizer& tokenizer);

}  // namespace mini_lexical_search
