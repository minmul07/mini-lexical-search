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

    // 로드에 실패한 스냅샷을 빈 문서 목록에서 다시 만든다.
    // 저장이 성공한 뒤에만 ready 상태가 된다. 실패하면 기존 파일과 미준비 상태를 유지한다.
    IndexUpdate rebuildIndex(const std::filesystem::path& directory,
                             const IndexProgressCallback& progress = {});

    // 점수 내림차순으로 상위 topK개를 반환한다. 동점은 문서 ID 오름차순이다.
    // topK가 0이면 빈 결과이다. maxMatchingLines가 0이면 모든 매칭 줄을 포함한다.
    // 매칭 줄 원문은 선정된 문서만 읽는다. 읽기 실패는 그 결과의 경고로 남긴다.
    // 초기 로드 실패 또는 잘못된 질의 시 예외를 던진다. snapshot을 저장하지 않는다.
    std::vector<SearchResult> search(const std::string& query,
                                     std::size_t maxMatchingLines = 0,
                                     std::size_t topK = 10);

private:
    void requireReady() const;

    std::filesystem::path snapshotPath_;
    IndexSnapshot snapshot_;
    SimpleTokenizer tokenizer_;
    bool ready_ = true;
    std::string loadError_;
};

}  // namespace mini_lexical_search
