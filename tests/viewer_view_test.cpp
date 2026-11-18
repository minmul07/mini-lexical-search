#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

#include "ftxui.hpp"
#include "mini_lexical_search/presentation/tui/viewer_view.hpp"

using namespace mini_lexical_search::tui;

namespace {
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
std::string joined(const PreviewRow& row, bool hits = false) {
    std::string text;
    for (const auto& part : row) if (!hits || part.highlighted) text += part.text;
    return text;
}
ftxui::Event wheel(ftxui::Mouse::Button button, bool control = false) {
    ftxui::Mouse mouse;
    mouse.button = button;
    mouse.control = control;
    return ftxui::Event::Mouse("", mouse);
}
void dimensions(const ViewerView& view, int width) {
    const auto rows = view.visibleRows();
    require(rows.size() <= static_cast<std::size_t>(view.height()), "viewport height exceeded");
    for (const auto& row : rows)
        require(ftxui::string_width(joined(row)) <= width, "viewport width exceeded");
}
void navigation() {
    std::vector<std::string> lines;
    for (int i = 1; i <= 100; ++i) lines.push_back("alpha " + std::to_string(i));
    for (int height : {1, 2, 5, 10, 20}) {
        ViewerView view;
        view.open("공백 경로.txt", lines, 40, "alpha");
        view.layout(80, height);
        const int top = static_cast<int>(view.topLine());
        const int target = 39 - top;
        require(target == std::min(5, (height - 1) / 2), "initial target position incorrect");
        require(view.numbers() && view.wrap(), "opening defaults incorrect");
        view.handleEvent(ftxui::Event::ArrowDown);
        require(static_cast<int>(view.topLine()) == 40 - target,
                "down must scroll one display row");
        view.handleEvent(ftxui::Event::ArrowUp);
        require(static_cast<int>(view.topLine()) == 39 - target,
                "up must scroll one display row");
        view.handleEvent(ftxui::Event::PageDown);
        require(static_cast<int>(view.topLine()) == 39 - target + height,
                "page down must use viewport height");
        view.handleEvent(ftxui::Event::PageUp);
        require(static_cast<int>(view.topLine()) == 39 - target,
                "page up must restore offset");
        view.handleEvent(wheel(ftxui::Mouse::WheelDown));
        require(static_cast<int>(view.topLine()) == 42 - target,
                "wheel must scroll three rows");
        view.handleEvent(wheel(ftxui::Mouse::WheelUp));
        require(static_cast<int>(view.topLine()) == 39 - target, "wheel up incorrect");
        view.handleEvent(ftxui::Event::End);
        require(static_cast<int>(view.topLine()) == 100 - height, "end must show file end");
        view.handleEvent(ftxui::Event::Home);
        require(view.topLine() == 0, "home must show file start");
        view.scroll(-1000);
        require(view.topLine() == 0, "scroll underflow");
    }
    ViewerView view;
    view.open("first", lines, 1, "alpha");
    view.layout(80, 20);
    require(view.topLine() == 0 && joined(view.visibleRows()[0]).starts_with("> "),
            "first line marker missing");
    view.open("last", lines, 100, "alpha");
    view.layout(80, 20);
    require(view.topLine() == 94 && joined(view.visibleRows()[5]).starts_with("> "),
            "last matching line must stay five rows below body top, allowing blank space below");
    view.handleEvent(ftxui::Event::ArrowUp);
    view.handleEvent(ftxui::Event::ArrowDown);
    require(view.topLine() == 94, "trailing space must remain scrollable after moving up");
    view.handleEvent(ftxui::Event::Home);
    require(view.topLine() == 0, "home must show file start for the last target");
    view.handleEvent(ftxui::Event::End);
    require(view.topLine() == 94, "end must preserve the initial bottom limit near the file end");
    view.open("changed", {"one", "two"}, 40, "alpha");
    view.layout(80, 20);
    require(!view.notice().empty() && joined(view.visibleRows()[1]).starts_with("> "),
            "missing line fallback");
    view.handleEvent(ftxui::Event::End);
    require(view.topLine() == 0, "end must stay at the start when the file is shorter than the viewport");
    view.handleEvent(ftxui::Event::PageDown);
    require(view.topLine() == 0, "page down must not pass the end of a short file");
    view.open("empty", {}, 1, "alpha");
    view.layout(1, 1);
    require(view.visibleRows().empty() && !view.notice().empty(), "empty file needs notice");
    view.showError("deleted", "cannot read document");
    view.layout(20, 3);
    require(view.path() == "deleted" && view.notice() == "cannot read document", "read error lost");
}

void wrappedNavigation() {
    std::vector<std::string> lines;
    for (char value : {'a', 'b', 'c', 'd', 'e'}) lines.emplace_back(51, value);
    ViewerView view;
    view.open("wrapped navigation", lines, 1, "zzz");
    view.layout(22, 5);
    require(view.topLine() == 0 && view.topGlyph() == 0, "wrapped navigation start incorrect");
    view.handleEvent(ftxui::Event::PageDown);
    require(view.topLine() == 1 && view.topGlyph() == 34,
            "page down must cross a wrapped line boundary");
    require(joined(view.visibleRows()[0]).substr(5).starts_with("bbb"),
            "page down landed on the wrong wrapped segment");
    view.handleEvent(ftxui::Event::Character('w'));
    require(!view.wrap() && view.topLine() == 1 && view.topGlyph() == 0,
            "wrap off must preserve the source line and reset the glyph");
    view.handleEvent(ftxui::Event::Character('w'));
    require(view.wrap() && view.topLine() == 1 && view.topGlyph() == 0,
            "wrap on must preserve the source line and start at its first segment");
    view.handleEvent(ftxui::Event::ArrowDown);
    view.handleEvent(ftxui::Event::ArrowDown);
    require(view.topLine() == 1 && view.topGlyph() == 34,
            "arrow down must reach the third wrapped row");
    view.handleEvent(ftxui::Event::Character('n'));
    require(view.topLine() == 1 && view.topGlyph() == 20,
            "number toggle must keep the glyph inside the new wrapped row");
    view.handleEvent(ftxui::Event::Character('n'));
    require(view.topLine() == 1 && view.topGlyph() == 17,
            "number toggle back must map the same glyph to its containing row");
    view.handleEvent(ftxui::Event::PageUp);
    require(view.topLine() == 0 && view.topGlyph() == 0, "page up must return to the start");
    view.handleEvent(ftxui::Event::ArrowDown);
    require(view.topLine() == 0 && view.topGlyph() == 17,
            "down must move one wrapped display row");
    view.handleEvent(ftxui::Event::ArrowUp);
    require(view.topLine() == 0 && view.topGlyph() == 0,
            "up must move one wrapped display row");
    view.handleEvent(ftxui::Event::End);
    require(view.topLine() == 3 && view.topGlyph() == 17, "end must show the last wrapped screen");
    require(joined(view.visibleRows()[0]).substr(5).starts_with("ddd"),
            "end landed on the wrong wrapped segment");
    view.handleEvent(ftxui::Event::Home);
    require(view.topLine() == 0 && view.topGlyph() == 0, "home must return to the start");
}

void wrapping() {
    ViewerView view;
    view.open("test", {"alpha ALPHA alphabet " + std::string(150, 'x'), "", "tail"}, 1, "alpha");
    view.layout(22, 30);
    std::string hits;
    for (const auto& row : view.visibleRows()) hits += joined(row, true);
    require(hits == "alphaALPHA", "whole token highlighting incorrect");
    require(view.visibleRows().size() > 3, "long line must wrap without excerpt limit");
    require(joined(view.visibleRows().back()).starts_with("  3  "),
            "blank line must preserve numbering");
    dimensions(view, 22);
    require(view.numbers(), "numbers must be on by default");
    require(joined(view.visibleRows()[0]).starts_with("> 1  "), "original line number missing");
    require(joined(view.visibleRows()[1]).starts_with("     "),
            "continuation must have blank gutter");
    view.handleEvent(ftxui::Event::Character('n'));
    require(!view.numbers(), "n must disable numbers");
    require(joined(view.visibleRows()[0]).starts_with("> "), "target marker must remain without numbers");
    dimensions(view, 22);
    view.handleEvent(ftxui::Event::Character('n'));
    require(view.numbers(), "n must enable numbers");
    dimensions(view, 22);
    view.handleEvent(ftxui::Event::Character('w'));
    require(!view.wrap() && view.visibleRows().size() == 3, "w must disable wrap");
    dimensions(view, 22);
    view.handleEvent(ftxui::Event::ArrowRight);
    require(view.horizontalOffset() == 1, "right must scroll one cell");
    view.handleEvent(wheel(ftxui::Mouse::WheelDown, true));
    require(view.horizontalOffset() == 4 && view.topLine() == 0,
            "Ctrl+wheel must only scroll horizontally");
    view.handleEvent(wheel(ftxui::Mouse::WheelUp, true));
    view.handleEvent(ftxui::Event::ArrowLeft);
    require(view.horizontalOffset() == 0, "horizontal reverse movement incorrect");
    view.scrollHorizontal(10000);
    require(view.horizontalOffset() > 0 && view.horizontalOffset() < 200,
            "horizontal end not clamped");
    dimensions(view, 22);
    view.handleEvent(ftxui::Event::Character('w'));
    require(view.wrap() && view.horizontalOffset() == 0, "enabling wrap must reset horizontal offset");
    view.handleEvent(wheel(ftxui::Mouse::WheelDown, true));
    require(view.horizontalOffset() == 0, "wrapped mode must ignore horizontal wheel");
    view.open("reopened", {"alpha"}, 1, "alpha");
    view.layout(20, 3);
    require(view.numbers() && view.wrap(), "reopening must reset preferences");

    ViewerView combining;
    combining.open("combining", {std::string("\xCC\x81") + "alpha beta"}, 1, "alpha");
    combining.layout(40, 4);
    require(joined(combining.visibleRows()[0], true) == "alpha",
            "leading combining mark must not shift highlight offsets");
}

void koreanShortcuts() {
    ViewerView view;
    view.open("korean", {"alpha"}, 1, "alpha");
    view.layout(20, 3);
    require(view.numbers() && view.wrap(), "opening defaults incorrect");
    view.handleEvent(ftxui::Event::Character("ㅜ"));
    require(!view.numbers() && view.wrap(), "Hangul n key must toggle numbers");
    view.handleEvent(ftxui::Event::Character("ㅜ"));
    require(view.numbers(), "Hangul n key must toggle numbers back");
    view.handleEvent(ftxui::Event::Character("ᅮ"));
    require(!view.numbers(), "conjoining Hangul n key must toggle numbers");
    view.handleEvent(ftxui::Event::Character("ㅈ"));
    require(!view.numbers() && !view.wrap(), "Hangul w key must toggle wrap");
    view.handleEvent(ftxui::Event::Character("ㅈ"));
    require(view.wrap(), "Hangul w key must toggle wrap back");
    view.handleEvent(ftxui::Event::Character("ᄌ"));
    require(!view.wrap(), "conjoining Hangul w key must toggle wrap");
    view.handleEvent(ftxui::Event::Character('n'));
    require(view.numbers(), "ASCII n key must still toggle numbers");
    view.handleEvent(ftxui::Event::Character('w'));
    require(view.wrap(), "ASCII w key must still toggle wrap");
}

void resizeAndUnicode() {
    ViewerView view;
    const std::string source = std::string(200, 'x') + " 자료구조 👨‍👩‍👧 👍🏽 🇰🇷 e\xCC\x81 끝";
    view.open("unicode", {"first", source, "", "last"}, 2, "자료구조");
    view.layout(22, 3);
    view.scroll(5);
    const std::size_t anchorLine = view.topLine();
    const std::size_t anchorGlyph = view.topGlyph();
    require(anchorLine == 1, "resize test expects the second source line");
    view.layout(12, 3);
    require(view.topLine() == anchorLine && view.topGlyph() <= anchorGlyph,
            "resize lost source anchor");
    const auto afterLine = view.topLine();
    const auto afterGlyph = view.topGlyph();
    view.layout(12, 3);
    require(view.topLine() == afterLine && view.topGlyph() == afterGlyph,
            "repeated layout must preserve scroll");
    view.handleEvent(ftxui::Event::Character('n'));
    require(view.topLine() == anchorLine, "number toggle lost source line");
    for (int width = 1; width <= 45; ++width) {
        view.open("unicode", {source, "\t\x1b[31m", ""}, 1, "자료구조");
        view.layout(width, 300);
        dimensions(view, width);
        for (const auto& row : view.visibleRows())
            require(joined(row).find('\x1b') == std::string::npos, "terminal escape leaked");
        view.toggleNumbers();
        dimensions(view, width);
        view.toggleWrap();
        dimensions(view, width);
        for (int cells = 0; cells < 12; ++cells) {
            view.scrollHorizontal(1);
            dimensions(view, width);
        }
    }
    view.open("wide", {"한글e\xCC\x81👨‍👩‍👧👍🏽🇰🇷"}, 1, "한글");
    view.toggleNumbers();
    view.layout(5, 30);
    std::string all;
    for (const auto& row : view.visibleRows()) all += joined(row).substr(2);
    require(all == "한글e\xCC\x81👨‍👩‍👧👍🏽🇰🇷", "wrapping split or lost grapheme clusters");
    view.toggleWrap();
    view.scrollHorizontal(1);
    require(joined(view.visibleRows()[0]).substr(2).starts_with(" "),
            "partial wide glyph must retain cell position");
}

void selection() {
    ViewerView view;
    view.open("select", {"alpha beta", "gamma delta", "마지막 줄"}, 1, "alpha");
    view.layout(40, 8);
    require(view.numbers(), "selection test expects default numbers");
    const std::string first = joined(view.visibleRows()[0]);
    const auto textStart = first.find("alpha");
    require(textStart != std::string::npos, "selection body start missing");
    const int gutter = static_cast<int>(textStart);

    view.beginSelection(gutter, 0);
    view.extendSelection(gutter + 8, 1);
    view.endSelection(gutter + 8, 1);
    require(view.hasSelection(), "drag must create a selection");
    require(view.selectedText() == "alpha beta\ngamma del", "selected text incorrect");
    require(view.selectedText().find('1') == std::string::npos,
            "line number leaked into selection");

    view.beginSelection(gutter, 0);
    view.endSelection(gutter, 0);
    require(!view.hasSelection() && view.selectedText().empty(), "click must not select");

    view.beginSelection(gutter, 0);
    view.extendSelection(gutter + 4, 0);
    view.endSelection(gutter + 4, 0);
    const auto highlighted = view.visibleRows();
    require(!highlighted.empty() && highlighted[0][0].text.starts_with("> 1"),
            "gutter must keep the marker");
    require(!highlighted[0][0].selected, "gutter must not be selected");
    bool bodySelected = false;
    for (const auto& part : highlighted[0]) bodySelected = bodySelected || part.selected;
    require(bodySelected, "body selection highlight missing");

    view.handleEvent(wheel(ftxui::Mouse::WheelDown));
    require(!view.hasSelection() && view.selectedText().empty(),
            "scrolling must clear selection");

    view.open("wide select", {"한글abc"}, 1, "zzz");
    view.layout(20, 4);
    const auto wideStart = static_cast<int>(joined(view.visibleRows()[0]).find("한"));
    view.beginSelection(wideStart, 0);
    view.extendSelection(wideStart + 1, 0);
    view.endSelection(wideStart + 1, 0);
    require(view.selectedText() == "한", "partial wide glyph selection must keep the glyph intact");
}
} // namespace

int main() {
    try {
        navigation();
        wrappedNavigation();
        wrapping();
        koreanShortcuts();
        resizeAndUnicode();
        selection();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
