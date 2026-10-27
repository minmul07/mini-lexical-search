#pragma once

#include <filesystem>
#include <functional>
#include <vector>
#include "mini_lexical_search/domain/document.hpp"

namespace mini_lexical_search {

// .txt/.md 파일을 재귀적으로 수집하고 토큰화 없이 원본 바이트의 해시를 계산한다.
std::vector<Document> collectDocuments(const std::filesystem::path& directory);

// 해시가 없는 기존 형식의 항목을 포함하여 문서 테이블을 읽는다.
std::vector<Document> loadDocuments(const std::filesystem::path& input);

// 완전한 JSON 파일을 기록한 뒤에만 문서 테이블을 교체한다.
void saveDocuments(const std::vector<Document>& documents,
                   const std::filesystem::path& output);

// 문서를 순차적으로 읽으며 1부터 시작하는 원본 줄 번호를 전달한다.
// 파일 열기·읽기 실패 시 예외를 던지며 콜백은 동기적으로 실행한다.
using DocumentLineSink = std::function<void(std::uint32_t, const std::string&)>;
void readDocumentLines(const Document& document, const DocumentLineSink& sink);

}  // namespace mini_lexical_search
