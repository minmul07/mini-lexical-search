#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "mini_lexical_search/application/search_application.hpp"

namespace mini_lexical_search::cli {

enum class OutputMode { NoTui, Tui };

enum class LineAction {
    Ignored,
    Ran,
    Quit,
};

enum class Display { Unchanged, Text, Results };
// TUI에서 지정하면 검색 페이지의 텍스트 직렬화를 생략한다.
// 일반 명령과 오류 메시지는 계속 output에 기록한다.
struct CommandFeedback {
    Display display = Display::Unchanged;
    std::optional<SearchPage> page;
    bool newSearch = false;
    std::string query;
    std::string elapsed;
};

enum class Navigation { More, Next, Previous };
// 버튼은 명령 문자열로 변환하지 않고 원래 경로를 그대로 전달한다.
void navigate(SearchApplication& application, Navigation navigation, const std::string& path,
              std::ostream& output, CommandFeedback* feedback = nullptr);

void printUsage(const char* program);
LineAction handleLine(const char* program, SearchApplication& application, std::string_view line,
                      std::ostream& output, OutputMode mode = OutputMode::NoTui,
                      CommandFeedback* feedback = nullptr);
int runRepl(const char* program, SearchApplication& application);

}  // namespace mini_lexical_search::cli
