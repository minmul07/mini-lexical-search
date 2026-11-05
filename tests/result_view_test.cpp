#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include "ftxui.hpp"
#include "mini_lexical_search/presentation/tui/result_view.hpp"

using namespace mini_lexical_search;
using namespace mini_lexical_search::tui;

void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
std::string joined(const std::vector<PreviewRow>& rows, bool highlightedOnly = false) {
    std::string text;
    for (const auto& row : rows) for (const auto& part : row)
        if (!highlightedOnly || part.highlighted) text += part.text;
    return text;
}
void dimensions(const std::vector<PreviewRow>& rows, int width, int limit = 3) {
    require(!rows.empty() && static_cast<int>(rows.size()) <= limit, "preview height exceeds limit");
    for (const auto& row : rows) {
        std::string text;
        for (const auto& part : row) text += part.text;
        require(ftxui::string_width(text) <= width, "preview exceeds terminal width");
    }
}
SearchPage page() {
    SearchPage page;
    SearchResult file;
    file.relativePath = " leading.txt";
    file.totalMatchingLines = 21;
    for (std::uint32_t n = 1; n <= 10; ++n)
        file.lines.push_back({n, "alpha " + std::string(150, 'x')});
    page.files.push_back(file);
    file.relativePath = "폴더/다음.md";
    file.lines = {{20, "ALPHA beta"}};
    file.totalMatchingLines = 1;
    page.files.push_back(file);
    page.hasNext = true;
    return page;
}
int main() {
    try {
        const auto exact = preview("AbC abcdef abc 자료구조", {"abc", "자료구조"}, 80);
        require(joined(exact) == "AbC abcdef abc 자료구조", "short source should remain intact");
        require(joined(exact, true) == "AbCabc자료구조", "highlight must use normalized whole tokens");
        const auto full = preview(std::string(30, 'a'), {}, 10);
        require(full.size() == 3 && joined(full) == std::string(30, 'a'), "exact three rows must not be truncated");
        const auto excerpt = preview(std::string(200, 'x') + " alpha " + std::string(200, 'z'), {"alpha"}, 20);
        dimensions(excerpt, 20);
        require(joined(excerpt).starts_with("…") && joined(excerpt).ends_with("…"), "excerpt must indicate omitted text");
        require(joined(excerpt, true) == "alpha", "late keyword must remain visible");
        const auto tail = preview(std::string(200, 'x') + " alpha", {"alpha"}, 20);
        require(joined(tail, true) == "alpha" && !joined(tail).ends_with("…"), "tail excerpt must include final match");
        const auto repeated = preview("alpha ALPHA alpha", {"alpha"}, 40);
        require(joined(repeated, true) == "alphaALPHAalpha", "all occurrences must be highlighted");
        for (int width = 1; width <= 40; ++width) {
            for (const auto& text : {std::string("앞뒤 자료구조 👨‍👩‍👧 👍🏽 🇰🇷 e\xCC\x81 끝"),
                                     std::string(120, 'x') + " alpha " + std::string(100, 'z')}) {
                for (int height = 1; height <= 3; ++height)
                    dimensions(preview(text, {"자료구조", "alpha"}, width, height), width, height);
            }
        }
        const auto glyphs = displayGlyphs("한e\xCC\x81👨‍👩‍👧👍🏽🇰🇷");
        require(glyphs == std::vector<std::string>({"한", "e\xCC\x81", "👨‍👩‍👧", "👍🏽", "🇰🇷"}),
                "combining marks and emoji clusters must stay together");
        require(joined(preview("alpha\t\x1b[31m", {"alpha"}, 80)).find('\x1b') == std::string::npos,
                "terminal control bytes must be sanitized");

        ResultView view;
        require(view.mode == ViewMode::Home, "initial screen is home");
        view.showResults(page(), "alpha AND beta", "0.1");
        view.layout(50, 18);
        require(view.items().size() == 13, "ten lines, more, second file line and next are selectable");
        require(view.items()[10].key.kind == ItemKind::More, "more follows file rows");
        require(std::count_if(view.rows().begin(), view.rows().end(), [](const auto& row) { return row.separator; }) == 1,
                "horizontal bar must separate files");
        view.move(7);
        const auto key = view.selection()->key;
        require(view.offset() > 0, "keyboard selection must scroll into view");
        view.scroll(-1000);
        require(view.offset() == 0 && view.selection()->key == key, "wheel scrolling must preserve selection without snapping back");
        view.layout(50, 18);
        require(view.offset() == 0, "rendering must not undo scrolling");
        view.select(key);
        view.layout(25, 4);
        require(view.selection()->key == key && view.height() == 4, "resize preserves selected logical row and uses current height");
        const int before = view.offset();
        view.scroll(view.height());
        require(view.offset() == before + 4, "page down after shrink uses current viewport");
        view.select(10000);
        require(view.selection()->key.kind == ItemKind::Next, "end selects next-page control");
        view.move(-1);
        require(view.selection()->key.path == "폴더/다음.md", "up moves to previous result");
        auto expanded = page();
        expanded.files[0].lines.push_back({42, "alpha newly expanded"});
        view.showResults(expanded, view.query, view.elapsed);
        view.layout(50, 10);
        view.select({ItemKind::Line, " leading.txt", 42});
        require(view.selection()->key.line == 42 && view.offset() > 0, "newly expanded row can be revealed by stable key");
        for (int width = 1; width <= 30; ++width) {
            view.layout(width, 4);
            for (const auto& row : view.rows()) {
                std::string text;
                for (const auto& part : row.fragments) text += part.text;
                require(ftxui::string_width(text) <= width, "result row must fit narrow terminal");
            }
        }
        SearchPage empty;
        view.showResults(empty, "missing", "0.0");
        view.layout(40, 5);
        require(view.items().empty() && !view.rows().empty(), "empty result still has a message");
        auto failed = page();
        failed.files[0].warning = "file unavailable";
        view.showResults(failed, "alpha", "0.0");
        view.layout(40, 5);
        require(std::none_of(view.items().begin(), view.items().end(), [](const auto& item) { return item.key.kind == ItemKind::More; }),
                "failed file cannot offer more");
        view.showText("error: invalid query", "alpha beta");
        view.layout(10, 4);
        require(view.mode == ViewMode::Text && view.items().empty(), "error replaces result selection");
        view.resultsFocused = true;
        view.home();
        require(view.mode == ViewMode::Home && view.query.empty() && view.page.files.empty() &&
                view.items().empty() && view.rows().empty() && view.offset() == 0 && !view.resultsFocused,
                "home clears query, page, expansion, selection and scroll");
        std::cout << "result_view_test passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
