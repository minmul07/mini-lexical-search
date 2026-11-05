#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace mini_lexical_search {

// [begin, end)는 원문 UTF-8 바이트 범위이며 normalized는 검색용 토큰이다.
struct TokenSpan {
    std::string normalized;
    std::size_t begin;
    std::size_t end;
};

// 한 줄의 텍스트를 검색 가능한 토큰으로 분리한다.
// 줄 번호는 파일을 읽는 호출자가 붙이므로 tokenizer는 줄 번호를 받지 않는다.
class Tokenizer {
public:
    virtual ~Tokenizer() = default;

    virtual std::vector<std::string> tokenize(const std::string& text) = 0;
};

// P0 tokenizer: 연속된 ASCII 숫자·영문(소문자로 변환)과 한글 code point
// (완성형 음절·자모)를 토큰으로 유지한다. 그 밖의 code point(공백, 문장부호,
// 이모지, BOM, 다른 언어 문자)는 토큰을 구분하고 결과에서 제외한다.
// 잘못된 UTF-8 바이트도 구분자로 처리한다.
class SimpleTokenizer final : public Tokenizer {
public:
    std::vector<std::string> tokenize(const std::string& text) override;
    std::vector<TokenSpan> tokenizeWithOffsets(const std::string& text);
};

}  // namespace mini_lexical_search
