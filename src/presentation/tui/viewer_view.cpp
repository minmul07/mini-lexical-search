#include "mini_lexical_search/presentation/tui/viewer_view.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "ftxui.hpp"
#include "mini_lexical_search/domain/query_processor.hpp"

namespace mini_lexical_search::tui {
namespace {
void append(PreviewRow& row, const std::string& text, bool highlighted = false) {
    if (!row.empty() && row.back().highlighted == highlighted) row.back().text += text;
    else row.push_back({text, highlighted});
}

// 셀 경계에서 잘리는 전각 문자는 공백으로 바꿔 뒤쪽 셀 위치를 유지한다.
PreviewRow slice(const PreviewRow& source, int left, int width) {
    PreviewRow result;
    int column = 0;
    for (const auto& part : source) {
        for (const auto& glyph : displayGlyphs(part.text)) {
            const int cells = std::max(1, ftxui::string_width(glyph));
            const int begin = std::max(left, column);
            const int end = std::min(left + width, column + cells);
            if (begin < end) append(result,
                begin == column && end == column + cells ? glyph : std::string(end - begin, ' '),
                part.highlighted);
            column += cells;
            if (column >= left + width) return result;
        }
    }
    return result;
}
} // namespace

void ViewerView::open(std::string path, std::vector<std::string> lines, std::uint32_t target,
                      const std::string& query) {
    *this = ViewerView{};
    path_ = std::move(path);
    lines_ = std::move(lines);
    SimpleTokenizer tokenizer;
    terms_ = parseQuery(query, tokenizer).terms;
    target_ = target > 0 ? static_cast<std::size_t>(target - 1) : 0;
    if (lines_.empty()) notice_ = "Empty file.";
    if (target_ >= lines_.size()) {
        notice_ = "Indexed line is missing; run /index again.";
        target_ = lines_.empty() ? 0 : lines_.size() - 1;
    }
}

void ViewerView::showError(std::string path, std::string error) {
    *this = ViewerView{};
    path_ = std::move(path);
    notice_ = std::move(error);
}

int ViewerView::gutter() const {
    const int marker = width_ >= 3 ? 2 : 0;
    const int number = numbers_ ? static_cast<int>(std::to_string(lines_.size()).size()) + 2 : 0;
    return std::min(width_ - 1, marker + number);
}
int ViewerView::bodyWidth() const { return std::max(1, width_ - gutter()); }

void ViewerView::layout(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (!initial_ && width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
    rebuild();
}

void ViewerView::rebuild() {
    std::size_t anchorLine = 0, anchorGlyph = 0;
    if (!rows_.empty()) {
        anchorLine = rows_[offset_].line;
        anchorGlyph = rows_[offset_].firstGlyph;
    }
    rows_.clear();
    maxLineWidth_ = 0;
    for (std::size_t line = 0; line < lines_.size(); ++line) {
        auto content = wrapped(lines_[line], terms_, wrap_ ? bodyWidth() : std::numeric_limits<int>::max());
        std::size_t glyph = 0;
        for (auto& fragments : content) {
            int cells = 0;
            std::size_t count = 0;
            for (const auto& part : fragments) {
                cells += ftxui::string_width(part.text);
                count += displayGlyphs(part.text).size();
            }
            maxLineWidth_ = std::max(maxLineWidth_, cells);
            rows_.push_back({std::move(fragments), line, glyph});
            glyph += count;
        }
    }
    offset_ = 0;
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        const auto& row = rows_[i];
        if (initial_ ? row.line == target_ : row.line == anchorLine && row.firstGlyph <= anchorGlyph) {
            offset_ = static_cast<int>(i);
            if (initial_) break;
        }
    }
    if (initial_) offset_ -= std::min(5, (height_ - 1) / 2);
    // 파일 끝에서도 진입 줄이 화면 아래로 밀리지 않도록 하단 빈 공간을 허용한다.
    maxOffset_ = std::max({0, static_cast<int>(rows_.size()) - height_, offset_});
    initial_ = false;
    scroll(0);
    scrollHorizontal(0);
}

void ViewerView::scroll(int rows) {
    offset_ = std::clamp(offset_ + rows, 0, maxOffset_);
}
void ViewerView::scrollHorizontal(int cells) {
    horizontal_ = wrap_ ? 0 : std::clamp(horizontal_ + cells, 0, std::max(0, maxLineWidth_ - bodyWidth()));
}
void ViewerView::toggleNumbers() { numbers_ = !numbers_; rebuild(); }
void ViewerView::toggleWrap() { wrap_ = !wrap_; rebuild(); }

void ViewerView::handleEvent(ftxui::Event event) {
    if (event == ftxui::Event::Character('n')) toggleNumbers();
    else if (event == ftxui::Event::Character('w')) toggleWrap();
    else if (event == ftxui::Event::ArrowUp) scroll(-1);
    else if (event == ftxui::Event::ArrowDown) scroll(1);
    else if (event == ftxui::Event::PageUp) scroll(-height_);
    else if (event == ftxui::Event::PageDown) scroll(height_);
    else if (event == ftxui::Event::Home) scroll(-offset_);
    else if (event == ftxui::Event::End) scroll(static_cast<int>(rows_.size()) - offset_);
    else if (event == ftxui::Event::ArrowLeft) scrollHorizontal(-1);
    else if (event == ftxui::Event::ArrowRight) scrollHorizontal(1);
    else if (event.is_mouse()) {
        const auto& mouse = event.mouse();
        if (mouse.button == ftxui::Mouse::WheelUp || mouse.button == ftxui::Mouse::WheelDown) {
            const int delta = mouse.button == ftxui::Mouse::WheelUp ? -3 : 3;
            if (mouse.control) scrollHorizontal(delta);
            else scroll(delta);
        } else if (mouse.button == ftxui::Mouse::WheelLeft || mouse.button == ftxui::Mouse::WheelRight) {
            scrollHorizontal(mouse.button == ftxui::Mouse::WheelLeft ? -3 : 3);
        }
    }
}

std::vector<PreviewRow> ViewerView::visibleRows() const {
    std::vector<PreviewRow> result;
    for (int i = offset_; i < static_cast<int>(rows_.size()) && i < offset_ + height_; ++i) {
        const auto& row = rows_[i];
        std::string prefix;
        if (width_ >= 3) prefix = row.line == target_ && row.firstGlyph == 0 ? "> " : "  ";
        if (numbers_) {
            const auto digits = std::to_string(lines_.size()).size();
            const auto number = row.firstGlyph == 0 ? std::to_string(row.line + 1) : "";
            prefix += std::string(digits - number.size(), ' ') + number + "  ";
        }
        prefix.resize(static_cast<std::size_t>(gutter()));
        PreviewRow content{{std::move(prefix), false}};
        for (const auto& part : slice(row.fragments, horizontal_, bodyWidth()))
            append(content, part.text, part.highlighted);
        result.push_back(std::move(content));
    }
    return result;
}

std::string ViewerView::footer() const {
    const auto line = rows_.empty() ? 0 : rows_[offset_].line + 1;
    const auto state = "N:" + std::string(numbers_ ? "on" : "off") +
                       " W:" + (wrap_ ? "on" : "off");
    if (width_ < 65) return "Esc:뒤로 ↑↓ PgUp/Dn n:번호 w:wrap " + state;
    if (width_ < 125) return "Esc:뒤로 ↑↓/휠 PgUp/Dn Home/End ←→/^휠 n:번호 w:wrap " + state;
    return "Esc:뒤로 ↑↓/휠:스크롤 PgUp/Dn Home/End ←→/Ctrl+휠:가로 n:번호 w:wrap " +
           state + " | " + std::to_string(line) + "/" + std::to_string(lines_.size());
}

} // namespace mini_lexical_search::tui
