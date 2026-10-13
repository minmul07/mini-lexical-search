#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mini_lexical_search {

using Sha256Digest = std::array<std::uint8_t, 32>;

// 점진적 SHA-256(FIPS 180-4). update()로 바이트 범위를 여러 번 전달한 뒤
// finish()를 한 번 호출하여 digest를 얻는다. streaming 방식으로 처리하므로
// 큰 문서 전체를 메모리에 보관하지 않고 해시를 계산한다.
class Sha256 {
public:
    Sha256();

    void update(const void* data, std::size_t size);
    Sha256Digest finish();

private:
    void transform(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state_;
    std::array<std::uint8_t, 64> buffer_;
    std::uint64_t length_ = 0;  // 전체 메시지 길이(바이트)
    std::size_t buffered_ = 0;  // 현재 buffer_에 보관된 바이트 수
};

// 단일 buffer의 해시를 한 번에 계산한다.
Sha256Digest sha256(const void* data, std::size_t size);

// digest를 소문자 16진수 문자열(64자)로 인코딩한다.
std::string toHex(const Sha256Digest& digest);

}  // namespace mini_lexical_search
