#pragma once

#include <filesystem>
#include <functional>
#include <vector>
#include "mini_lexical_search/domain/document.hpp"
#include "mini_lexical_search/domain/tokenizer.hpp"

namespace mini_lexical_search {

struct IndexStats {
    std::size_t notUpdated = 0;
    std::size_t updated = 0;
    std::size_t added = 0;
    std::size_t deleted = 0;
};

// 토큰화 전에 저장된 문서 테이블과 디렉터리를 비교한 결과이다.
// 변경 없는 문서는 기존 ID와 tokenCount를 유지한다.
struct IndexWork {
    std::vector<Document> documents;
    // 토큰화가 필요한 추가·변경 문서의 `documents` 내 인덱스 목록이다.
    // 모든 문서에 변경이 없으면 비어 있다.
    std::vector<std::size_t> toTokenize;
    // 기존 posting을 제거해야 하는 변경·삭제 문서의 ID를 오름차순으로 담는다.
    // 갱신할 역색인 항목이 없으면 비어 있다.
    std::vector<DocumentId> removed;
    IndexStats stats;
    bool changed = false;  // 저장된 테이블을 다시 기록해야 하는지 나타낸다.
};

struct TokenizeStats {
    std::size_t documents = 0;
    std::uint64_t tokens = 0;
};

// 토큰화로 생성된 모든 토큰을 문서 ID와 원본 줄 번호(1부터 시작)와 함께 받는다.
// 메타데이터만 처리하는 호출자는 빈 콜백을 사용한다.
using TokenSink = std::function<void(DocumentId, std::uint32_t, const std::string&)>;

// 선택한 디렉터리와 기존 문서 테이블을 비교하여 각 문서를 분류한다.
// 디스크 기록이나 토큰화는 수행하지 않는다.
IndexWork prepareIndex(const std::filesystem::path& directory,
                       const std::vector<Document>& previous);

// JSON 파일에서 기존 테이블을 로드한 뒤 동일한 비교를 수행한다.
// 파일이 없으면 빈 테이블로 취급하고 항상 변경된 작업으로 표시한다.
IndexWork prepareIndex(const std::filesystem::path& directory,
                       const std::filesystem::path& output);

// 선택한 문서를 줄 단위로 토큰화하여 `tokenCount`를 기록하고,
// `sink`가 설정되어 있으면 모든 토큰을 전달한다. 지정된 문서만 읽으며,
// 호출자는 변경 없는 문서를 선택 대상에서 제외한다.
TokenizeStats tokenizeDocuments(std::vector<Document>& documents,
                                const std::vector<std::size_t>& selection,
                                Tokenizer& tokenizer,
                                const TokenSink& sink);

// SimpleTokenizer로 전체 파이프라인을 실행한다. 문서 테이블을 동기화하고
// 변경 문서를 토큰화하며, 테이블이 변경되었으면 저장한다.
IndexStats indexDocuments(const std::filesystem::path& directory,
                          const std::filesystem::path& output);

}  // namespace mini_lexical_search
