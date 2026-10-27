#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

#include "mini_lexical_search/domain/document.hpp"

namespace mini_lexical_search::json {

// JSON 기본값을 엄격하게 파싱하며 텍스트의 현재 위치를 추적하는 cursor이다.
// `label`은 오류 메시지에 표시할 형식 이름이다. 예: "document", "index".
class Reader {
public:
    Reader(std::string text, std::string label);

    // 공백을 건너뛰고 다음 문자가 `value`이면 소비한다.
    bool take(char value);

    void expect(char value);
    std::string string();
    std::uint32_t unsignedInteger();

    // 파싱된 값 뒤에 남은 데이터가 있으면 거부한다.
    void finish();

    [[noreturn]] void fail(const std::string& reason) const;

private:
    void whitespace();
    unsigned int hexCode();
    void unicode(std::string& value);

    std::string text_;
    std::string label_;
    std::size_t position_ = 0;
};

// JSON 문자열 리터럴을 기록하며 따옴표, 역슬래시, 제어 문자를 escape 처리한다.
// 0x20 이상인 바이트는 그대로 출력하여 저장·로드 후에도 UTF-8 텍스트를 유지한다.
void writeString(std::ostream& output, const std::string& value);

// 들여쓰기 없이 Document 객체 하나를 기록한다.
void writeDocument(std::ostream& output, const Document& document);

// `{...}` 형식의 Document 객체 하나를 파싱한다.
Document parseDocument(Reader& reader);

// `[...]` 형식의 Document 배열을 파싱하며 중복 경로와 ID를 거부한다.
std::vector<Document> parseDocumentArray(Reader& reader);

// staging directory를 거쳐 `output`을 기록하여 쓰기 실패 시 기존 파일을 보존한다.
// `write`에는 바이너리 모드로 열고 기존 내용을 비운 출력 stream을 전달한다.
void writeFileAtomically(const std::filesystem::path& output,
                         const std::function<void(std::ostream&)>& write);

}  // namespace mini_lexical_search::json
