#pragma once

#include <iosfwd>
#include <string_view>

#include "mini_lexical_search/application/search_application.hpp"

namespace mini_lexical_search::cli {

enum class LineAction {
    Ignored,
    Ran,
    Quit,
};

void printUsage(const char* program);
LineAction handleLine(const char* program, SearchApplication& application, std::string_view line,
                      std::ostream& output);
void runRepl(const char* program, SearchApplication& application);

}  // namespace mini_lexical_search::cli
