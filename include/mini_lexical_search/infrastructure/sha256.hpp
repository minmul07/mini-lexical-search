#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mini_lexical_search {

using Sha256Digest = std::array<std::uint8_t, 32>;

// Incremental SHA-256 (FIPS 180-4). Feed any number of byte ranges with
// update() and call finish() once to obtain the digest. Kept streaming so large
// documents are hashed without holding them in memory.
class Sha256 {
public:
    Sha256();

    void update(const void* data, std::size_t size);
    Sha256Digest finish();

private:
    void transform(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state_;
    std::array<std::uint8_t, 64> buffer_;
    std::uint64_t length_ = 0;  // total message length in bytes
    std::size_t buffered_ = 0;  // bytes currently in buffer_
};

// One-shot hash of a single buffer.
Sha256Digest sha256(const void* data, std::size_t size);

// Lowercase hex encoding of a digest (64 characters).
std::string toHex(const Sha256Digest& digest);

}  // namespace mini_lexical_search
