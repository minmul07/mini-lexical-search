#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "mini_lexical_search/presentation/tui/clipboard.hpp"

using namespace mini_lexical_search::tui;

namespace {
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

std::string payload(const std::string& sequence) {
    const std::string prefix = "\x1b]52;c;";
    const std::string suffix = "\a";
    require(sequence.starts_with(prefix), "OSC 52 prefix missing");
    require(sequence.ends_with(suffix), "OSC 52 terminator missing");
    return sequence.substr(prefix.size(), sequence.size() - prefix.size() - suffix.size());
}

void encoding() {
    require(payload(osc52ClipboardSequence("")) == "", "empty base64");
    require(payload(osc52ClipboardSequence("f")) == "Zg==", "one byte base64");
    require(payload(osc52ClipboardSequence("fo")) == "Zm8=", "two byte base64");
    require(payload(osc52ClipboardSequence("foo")) == "Zm9v", "three byte base64");
    require(payload(osc52ClipboardSequence("foobar")) == "Zm9vYmFy", "six byte base64");
    require(payload(osc52ClipboardSequence("한글")) == "7ZWc6riA", "utf8 base64");
}

void writing() {
    std::ostringstream captured;
    auto* original = std::cout.rdbuf(captured.rdbuf());
    const bool copied = copyToClipboard("hello");
    std::cout.rdbuf(original);
    require(copied, "copyToClipboard must report success");
    require(captured.str() == osc52ClipboardSequence("hello"), "clipboard output mismatch");
    require(!copyToClipboard(""), "empty copy must be rejected");
}
} // namespace

int main() {
    try {
        encoding();
        writing();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
