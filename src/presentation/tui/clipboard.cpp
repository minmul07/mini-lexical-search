#include "mini_lexical_search/presentation/tui/clipboard.hpp"

#include <iostream>

namespace mini_lexical_search::tui {
namespace {

std::string base64(std::string_view input) {
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve((input.size() + 2) / 3 * 4);
    std::size_t index = 0;
    while (index + 3 <= input.size()) {
        const auto first = static_cast<unsigned char>(input[index]);
        const auto second = static_cast<unsigned char>(input[index + 1]);
        const auto third = static_cast<unsigned char>(input[index + 2]);
        output.push_back(kTable[first >> 2]);
        output.push_back(kTable[((first & 0x03) << 4) | (second >> 4)]);
        output.push_back(kTable[((second & 0x0F) << 2) | (third >> 6)]);
        output.push_back(kTable[third & 0x3F]);
        index += 3;
    }
    const auto remaining = input.size() - index;
    if (remaining == 1) {
        const auto first = static_cast<unsigned char>(input[index]);
        output.push_back(kTable[first >> 2]);
        output.push_back(kTable[(first & 0x03) << 4]);
        output += "==";
    } else if (remaining == 2) {
        const auto first = static_cast<unsigned char>(input[index]);
        const auto second = static_cast<unsigned char>(input[index + 1]);
        output.push_back(kTable[first >> 2]);
        output.push_back(kTable[((first & 0x03) << 4) | (second >> 4)]);
        output.push_back(kTable[(second & 0x0F) << 2]);
        output.push_back('=');
    }
    return output;
}

} // namespace

std::string osc52ClipboardSequence(std::string_view text) {
    return "\x1b]52;c;" + base64(text) + '\a';
}

bool copyToClipboard(std::string_view text) {
    if (text.empty()) return false;
    std::cout << osc52ClipboardSequence(text) << std::flush;
    return std::cout.good();
}

} // namespace mini_lexical_search::tui
