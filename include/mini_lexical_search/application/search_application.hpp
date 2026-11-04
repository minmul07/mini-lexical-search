#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
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
    // 완료 단계의 경과 시간이다. 시작 단계는 0이다.
    std::int64_t elapsedMicroseconds = 0;
    IndexStats metadata;
    TokenizeStats tokens;
};

// 동기적으로 호출한다. 애플리케이션을 재호출하거나 동시에 접근하지 않는다.
// 예외 발생 시 인덱싱을 중단하고, 재구축된 posting을 복구한 뒤 예외를 전달한다.
using IndexProgressCallback = std::function<void(const IndexProgress&)>;

struct SearchResult {
    Document document;
    std::string relativePath;
    std::size_t totalMatchingLines = 0;
    std::vector<MatchingLine> lines;
    // 문서 읽기가 실패해도 다른 결과는 유지한다.
    std::string warning;
};

struct SearchPage {
    std::vector<SearchResult> files;
    std::size_t number = 1;
    bool hasPrevious = false;
    bool hasNext = false;
};

// UI에 독립적인 동기식 검색 세션이다. 로드된 역색인과 tokenizer를 소유한다.
// 변경 가능한 snapshot 상태를 노출하지 않고 결과를 값으로 반환한다.
class SearchApplication {
public:
    explicit SearchApplication(std::filesystem::path snapshotPath);
    ~SearchApplication();

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

    // 파일 추가 전 50행을 확인하고 파일마다 처음 최대 10행을 표시한다.
    SearchPage search(const std::string& query);
    // 새 검색/인덱싱은 이전 세션을 지운다. 빈 검색 입력도 명시적으로 지울 수 있다.
    void clearSearch();
    SearchPage currentPage() const;
    SearchPage nextPage();
    SearchPage previousPage();
    SearchPage expandFile(const std::string& relativePath);

    // 동기적으로 호출하며 콜백에서 앱을 재호출하거나 동시에 접근하지 않는다.
    // 원문을 캐시하지 않는 전체 출력. 콜백 예외는 호출자에게 전달한다.
    // group은 파일당 한 번, line은 매칭 행당 한 번, warning은 읽기 실패 시 호출한다.
    void visitAllResults(const std::string& query,
                         const std::function<void(const SearchResult&)>& group,
                         const std::function<void(const MatchingLine&)>& line,
                         const std::function<void(const std::string&)>& warning);

private:
    void requireReady() const;
    void requireSearch() const;
    struct Session;
    std::unique_ptr<Session> session_;
    void startSearch(const std::string& query);
    void addPage();

    std::filesystem::path snapshotPath_;
    IndexSnapshot snapshot_;
    SimpleTokenizer tokenizer_;
    bool ready_ = true;
    std::string loadError_;
};

}  // namespace mini_lexical_search
