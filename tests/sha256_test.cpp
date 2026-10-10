#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "mini_lexical_search/sha256.hpp"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string hashString(const std::string& text) {
    return mini_lexical_search::toHex(
        mini_lexical_search::sha256(text.data(), text.size()));
}

// Feeds the same bytes through the incremental API in fixed-size chunks.
std::string hashInChunks(const std::string& text, std::size_t chunk) {
    mini_lexical_search::Sha256 hasher;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const std::size_t size = std::min(chunk, text.size() - offset);
        hasher.update(text.data() + offset, size);
        offset += size;
    }
    return mini_lexical_search::toHex(hasher.finish());
}

}  // namespace

int main() {
    try {
        // FIPS 180-4 / NIST example vectors.
        require(hashString("") ==
                    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                "empty message vector failed");
        require(hashString("abc") ==
                    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                "abc message vector failed");
        require(hashString("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
                    "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                "448-bit message vector failed");
        require(hashString("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                           "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu") ==
                    "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1",
                "896-bit message vector failed");
        require(hashString(std::string(1000000, 'a')) ==
                    "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
                "one million character vector failed");

        // Padding boundaries around the 56 mod 64 length field.
        const std::pair<std::size_t, const char*> boundaries[] = {
            {55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
            {56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
            {57, "f13b2d724659eb3bf47f2dd6af1accc87b81f09f59f2b75e5c0bed6589dfe8c6"},
            {63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
            {64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
            {65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"},
            {119, "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb"},
            {120, "2f3d335432c70b580af0e8e1b3674a7c020d683aa5f73aaaedfdc55af904c21c"},
            {128, "6836cf13bac400e9105071cd6af47084dfacad4e5e302c94bfed24e013afb73e"},
        };
        for (const auto& [length, expected] : boundaries) {
            require(hashString(std::string(length, 'a')) == expected,
                    "padding boundary vector failed at length " +
                        std::to_string(length));
        }

        // Splitting the input across update() calls must not change the digest,
        // including byte-by-byte and chunk sizes that straddle block limits.
        const std::string samples[] = {
            "",
            "abc",
            "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
            "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
            std::string(150000, 'x'),
        };
        for (const auto& sample : samples) {
            const std::string expected = hashString(sample);
            for (const std::size_t chunk : {1u, 3u, 63u, 64u, 65u, 1024u}) {
                require(hashInChunks(sample, chunk) == expected,
                        "chunked hashing differed from one-shot");
            }
        }

        // Digest hex encoding is always 64 lowercase characters.
        const std::string hex = hashString("abc");
        require(hex.size() == 64 &&
                    hex.find_first_not_of("0123456789abcdef") == std::string::npos,
                "digest hex encoding failed");

        std::cout << "sha256_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
