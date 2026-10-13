#include <cstring>
#include <iostream>

#include "mini_lexical_search/domain/version.hpp"

int main() {
    if (std::strcmp(mini_lexical_search::versionString(), "0.1.0") != 0) {
        std::cerr << "unexpected version: " << mini_lexical_search::versionString() << "\n";
        return 1;
    }
    std::cout << "version_test passed\n";
    return 0;
}
