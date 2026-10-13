#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "mini_lexical_search/application/indexer.hpp"
#include "mini_lexical_search/domain/matching_line.hpp"

namespace mini_lexical_search {

enum class IndexStage {
    MetadataStarted,
    MetadataCompleted,
    TokenizationStarted,
    TokenizationCompleted,
};

struct IndexProgress {
    IndexStage stage;
    std::int64_t elapsedMilliseconds = 0;
    IndexStats metadata;
    TokenizeStats tokens;
};

// 동기적으로 호출한다. 애플리케이션을 재호출하거나 동시에 접근하지 않는다.
// 예외 발생 시 인덱싱을 중단하고, 재구축된 posting을 복구한 뒤 예외를 전달한다.
using IndexProgressCallback = std::function<void(const IndexProgress&)>;

struct SearchResult {
    Document document;
    double score = 0;
    std::size_t totalMatchingLines = 0;
    std::vector<MatchingLine> lines;
    // 문서 읽기가 실패해도 다른 결과는 유지한다.
    std::string warning;
};

// UI에 독립적인 동기식 검색 세션이다. 로드된 역색인과 tokenizer를 소유한다.
// 변경 가능한 snapshot 상태를 노출하지 않고 결과를 값으로 반환한다.
class SearchApplication {
public:
    explicit SearchApplication(std::filesystem::path snapshotPath);

    bool ready() const;
    const std::string& loadError() const;
    const std::filesystem::path& snapshotPath() const;

    // 초기 로드, 인덱싱, 저장 또는 콜백 실패 시 예외를 던진다.
    // 완료 이벤트는 해당 단계의 완료를 나타내며 저장 성공을 의미하지 않는다.
    IndexUpdate index(const std::filesystem::path& directory,
                      const IndexProgressCallback& progress = {});

    // 문서 ID순으로 결과를 반환한다. 한도가 0이면 모든 매칭 줄을 포함한다.
    // 초기 로드 실패 또는 잘못된 질의 시 예외를 던진다. snapshot을 저장하지 않는다.
    std::vector<SearchResult> search(const std::string& query,
                                     std::size_t maxMatchingLines = 0);

private:
    void requireReady() const;

    std::filesystem::path snapshotPath_;
    IndexSnapshot snapshot_;
    SimpleTokenizer tokenizer_;
    bool ready_ = true;
    std::string loadError_;
};

}  // namespace mini_lexical_search
