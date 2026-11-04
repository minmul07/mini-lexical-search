#pragma once

#include <iosfwd>
#include <string_view>

#include "mini_lexical_search/application/search_application.hpp"

namespace mini_lexical_search::cli {

enum class OutputMode { NoTui, Tui };

enum class LineAction {
    Ignored,
    Ran,
    Quit,
};

void printUsage(const char* program);
LineAction handleLine(const char* program, SearchApplication& application, std::string_view line,
                      std::ostream& output, OutputMode mode = OutputMode::NoTui);
int runRepl(const char* program, SearchApplication& application);

}  // namespace mini_lexical_search::cli
