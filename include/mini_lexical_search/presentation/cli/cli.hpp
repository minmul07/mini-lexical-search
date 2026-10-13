#pragma once

#include "mini_lexical_search/application/search_application.hpp"

namespace mini_lexical_search::cli {

void printUsage(const char* program);
void runRepl(const char* program, SearchApplication& application);

}  // namespace mini_lexical_search::cli
