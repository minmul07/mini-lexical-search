#include "mini_lexical_search/presentation/tui/viewer_view.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "ftxui.hpp"
#include "mini_lexical_search/domain/query_processor.hpp"

namespace mini_lexical_search::tui {
namespace {
void append(PreviewRow& row, const std::string& text, bool highlighted = false,
            bool selected = false) {
    if (!row.empty() && row.back().highlighted == highlighted &&
        row.back().selected == selected) row.back().text += text;
    else row.push_back({text, highlighted, selected});
}

bool hasControl(const std::string& text) {
    return std::any_of(text.begin(), text.end(), [](char value) {
        const auto byte = static_cast<unsigned char>(value);
        return byte < 32 || byte == 127;
    });
}

void sanitize(std::string& text) {
    for (char& value : text) {
        const auto byte = static_cast<unsigned char>(value);
        if (byte < 32 || byte == 127) value = ' ';
    }
}

// 셀 경계에서 잘리는 전각 문자는 공백으로 바꿔 뒤쪽 셀 위치를 유지한다.
PreviewRow slice(const PreviewRow& source, int left, int width, int selectedLeft = 0,
                 int selectedRight = -1) {
    PreviewRow result;
    int column = 0;
    for (const auto& part : source) {
        for (const auto& glyph : displayGlyphs(part.text)) {
            const int cells = std::max(1, ftxui::string_width(glyph));
            const int begin = std::max(left, column);
            const int end = std::min(left + width, column + cells);
            if (begin < end) {
                const bool selected = selectedRight >= selectedLeft &&
                    column + cells - 1 >= selectedLeft && column <= selectedRight;
                append(result,
                    begin == column && end == column + cells ? glyph : std::string(end - begin, ' '),
                    part.highlighted, selected);
            }
            column += cells;
            if (column >= left + width) return result;
        }
    }
    return result;
}

// 두벌식 한글 입력 상태에서는 단축키가 같은 키의 자모로 전달된다.
char shortcutKey(const ftxui::Event& event) {
    if (!event.is_character()) return '\0';
    const auto& key = event.input();
    if (key == "n" || key == "ㅜ" || key == "ᅮ") return 'n';
    if (key == "w" || key == "ㅈ" || key == "ᄌ") return 'w';
    return '\0';
}
} // namespace

void ViewerView::open(std::string path, std::vector<std::string> lines, std::uint32_t target,
                      const std::string& query) {
    *this = ViewerView{};
    path_ = std::move(path);
    lines_ = std::move(lines);
    SimpleTokenizer tokenizer;
    const auto parsed = parseQuery(query, tokenizer);
    terms_.insert(parsed.terms.begin(), parsed.terms.end());
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
    resetLayout();
}

// 한 줄을 표시 행 경계로 나눈다. 측정 모드는 토큰·강조·fragment를 만들지 않아
// 스크롤 위치 계산과 높이 측정을 가볍게 유지한다.
ViewerView::LineLayout ViewerView::scanLine(std::size_t line, bool materialize) const {
    const std::string& raw = lines_[line];
    std::string sanitized;
    const std::string* text = &raw;
    if (hasControl(raw)) {
        sanitized = raw;
        sanitize(sanitized);
        text = &sanitized;
    }

    std::vector<TokenSpan> tokens;
    if (materialize && !terms_.empty()) {
        SimpleTokenizer tokenizer;
        tokens = tokenizer.tokenizeWithOffsets(*text);
    }
    const auto glyphs = ftxui::Utf8ToGlyphs(*text);
    const int limit = wrap_ ? bodyWidth() : std::numeric_limits<int>::max();
    LineLayout result;
    result.starts.push_back(0);
    if (materialize) result.segments.emplace_back();

    int used = 0;
    std::size_t glyphIndex = 0;
    std::size_t token = 0;
    std::size_t at = 0;
    for (std::size_t i = 0; i < glyphs.size(); ++i) {
        if (glyphs[i].empty()) continue; // 전각 글리프의 두 번째 셀
        // Utf8ToGlyphs는 폭 2 글리프 뒤에 빈 continuation 셀을 붙인다.
        const bool wide = i + 1 < glyphs.size() && glyphs[i + 1].empty();
        int width = wide ? 2 : 1;
        bool hit = false;
        std::string value;
        if (materialize) {
            // 폭 0 클러스터를 건너뛰므로 바이트 위치는 이전 글리프 끝에서 찾는다.
            const auto begin = text->find(glyphs[i], at);
            if (begin == std::string::npos) continue;
            const auto end = begin + glyphs[i].size();
            while (token < tokens.size() && tokens[token].end <= begin) ++token;
            hit = token < tokens.size() && tokens[token].begin < end &&
                  tokens[token].end > begin && terms_.contains(tokens[token].normalized);
            value = glyphs[i];
            at = end;
        }
        if (width > limit) {
            width = 1;
            if (materialize) value = "�";
        }
        if (used + width > limit) {
            result.starts.push_back(glyphIndex);
            if (materialize) result.segments.emplace_back();
            used = 0;
        }
        if (materialize) append(result.segments.back(), value, hit);
        used += width;
        ++glyphIndex;
    }
    return result;
}

std::size_t ViewerView::segmentIndex(const LineLayout& layout, std::size_t glyph) const {
    const auto it = std::upper_bound(layout.starts.begin(), layout.starts.end(), glyph);
    const auto index = static_cast<std::size_t>(it - layout.starts.begin());
    return index == 0 ? 0 : index - 1;
}

ViewerView::Position ViewerView::segmentPosition(std::size_t line, std::size_t glyph) const {
    if (lines_.empty()) return {};
    line = std::min(line, lines_.size() - 1);
    const auto layout = scanLine(line, false);
    return {line, layout.starts[segmentIndex(layout, glyph)]};
}

ViewerView::Position ViewerView::initialTop() const {
    if (lines_.empty()) return {};
    return retreat({target_, 0}, std::min(5, (height_ - 1) / 2));
}

ViewerView::Position ViewerView::lastRow() const {
    if (lines_.empty()) return {};
    const auto line = lines_.size() - 1;
    const auto layout = scanLine(line, false);
    return {line, layout.starts.back()};
}

ViewerView::Position ViewerView::retreat(Position from, int rows) const {
    if (lines_.empty() || rows <= 0) return lines_.empty() ? Position{} : from;
    from.line = std::min(from.line, lines_.size() - 1);
    std::size_t remaining = static_cast<std::size_t>(rows);
    auto layout = scanLine(from.line, false);
    std::size_t index = segmentIndex(layout, from.glyph);
    if (remaining <= index) return {from.line, layout.starts[index - remaining]};
    remaining -= index + 1;
    while (from.line > 0) {
        --from.line;
        layout = scanLine(from.line, false);
        if (remaining < layout.starts.size()) {
            return {from.line, layout.starts[layout.starts.size() - 1 - remaining]};
        }
        remaining -= layout.starts.size();
    }
    return {0, 0};
}

ViewerView::Position ViewerView::advance(Position from, int rows) const {
    if (lines_.empty() || rows <= 0) return from;
    from.line = std::min(from.line, lines_.size() - 1);
    std::size_t remaining = static_cast<std::size_t>(rows);
    auto layout = scanLine(from.line, false);
    std::size_t index = segmentIndex(layout, from.glyph);
    const std::size_t following = layout.starts.size() - 1 - index;
    if (remaining <= following) return {from.line, layout.starts[index + remaining]};
    remaining -= following + 1;
    while (from.line + 1 < lines_.size()) {
        ++from.line;
        layout = scanLine(from.line, false);
        if (remaining < layout.starts.size()) return {from.line, layout.starts[remaining]};
        remaining -= layout.starts.size();
    }
    return {from.line, layout.starts.back()};
}

// 파일 끝 상단은 첫 아래 이동/End에서만 계산한다. 마지막 화면과 마지막
// 레이아웃 시점의 상단 중 더 아래쪽을 허용해 기존 빈 공간 동작을 유지한다.
void ViewerView::ensureMaxTop() {
    if (maxTopKnown_) return;
    const auto endTop = retreat(lastRow(), height_ - 1);
    maxTop_ = resetTop_ < endTop ? endTop : resetTop_;
    maxTopKnown_ = true;
    if (maxTop_ < top_) top_ = maxTop_;
}

void ViewerView::ensureMaxLineWidth() {
    if (maxLineWidthKnown_) return;
    maxLineWidth_ = 0;
    std::string sanitized;
    for (const auto& line : lines_) {
        const std::string* text = &line;
        if (hasControl(line)) {
            sanitized = line;
            sanitize(sanitized);
            text = &sanitized;
        }
        maxLineWidth_ = std::max(maxLineWidth_, ftxui::string_width(*text));
    }
    maxLineWidthKnown_ = true;
}

void ViewerView::resetLayout() {
    clearSelection();
    visibleValid_ = false;
    if (initial_) {
        top_ = initialTop();
    } else {
        top_ = segmentPosition(top_.line, top_.glyph);
    }
    resetTop_ = top_;
    maxTop_ = {};
    maxTopKnown_ = false;
    if (wrap_) {
        horizontal_ = 0;
    } else if (horizontal_ > 0) {
        ensureMaxLineWidth();
        horizontal_ = std::min(horizontal_, std::max(0, maxLineWidth_ - bodyWidth()));
    }
    initial_ = false;
}

void ViewerView::scroll(int rows) {
    clearSelection();
    if (rows > 0) {
        ensureMaxTop();
        top_ = advance(top_, rows);
        if (maxTop_ < top_) top_ = maxTop_;
    } else if (rows < 0) {
        const int magnitude = rows == std::numeric_limits<int>::min()
            ? std::numeric_limits<int>::max()
            : -rows;
        top_ = retreat(top_, magnitude);
    }
    visibleValid_ = false;
}

void ViewerView::scrollHorizontal(int cells) {
    clearSelection();
    if (wrap_) {
        horizontal_ = 0;
        return;
    }
    ensureMaxLineWidth();
    horizontal_ = std::clamp(horizontal_ + cells, 0, std::max(0, maxLineWidth_ - bodyWidth()));
}

void ViewerView::jumpHome() {
    clearSelection();
    top_ = {};
    visibleValid_ = false;
}

void ViewerView::jumpEnd() {
    clearSelection();
    ensureMaxTop();
    top_ = maxTop_;
    visibleValid_ = false;
}

void ViewerView::toggleNumbers() {
    numbers_ = !numbers_;
    resetLayout();
}
void ViewerView::toggleWrap() {
    wrap_ = !wrap_;
    resetLayout();
}

void ViewerView::handleEvent(ftxui::Event event) {
    clearSelection();
    const char key = shortcutKey(event);
    if (key == 'n') toggleNumbers();
    else if (key == 'w') toggleWrap();
    else if (event == ftxui::Event::ArrowUp) scroll(-1);
    else if (event == ftxui::Event::ArrowDown) scroll(1);
    else if (event == ftxui::Event::PageUp) scroll(-height_);
    else if (event == ftxui::Event::PageDown) scroll(height_);
    else if (event == ftxui::Event::Home) jumpHome();
    else if (event == ftxui::Event::End) jumpEnd();
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

void ViewerView::ensureVisible() const {
    if (visibleValid_) return;
    visible_.clear();
    if (initial_ || lines_.empty()) {
        visibleValid_ = true;
        return;
    }
    visible_.reserve(static_cast<std::size_t>(height_));
    Position position = top_;
    while (position.line < lines_.size() &&
           visible_.size() < static_cast<std::size_t>(height_)) {
        auto layout = scanLine(position.line, true);
        std::size_t index = segmentIndex(layout, position.glyph);
        for (; index < layout.segments.size() &&
               visible_.size() < static_cast<std::size_t>(height_); ++index) {
            visible_.push_back({std::move(layout.segments[index]), position.line,
                                layout.starts[index]});
        }
        ++position.line;
        position.glyph = 0;
    }
    visibleValid_ = true;
}

ViewerView::SelectionPoint ViewerView::pointAt(int column, int row) const {
    ensureVisible();
    if (visible_.empty()) return {};
    const int last = static_cast<int>(visible_.size()) - 1;
    const int displayRow = std::clamp(row, 0, last);
    const int bodyColumn = std::clamp(column - gutter(), 0, std::max(0, bodyWidth() - 1));
    return {displayRow, horizontal_ + bodyColumn};
}

std::pair<ViewerView::SelectionPoint, ViewerView::SelectionPoint>
ViewerView::selectionRange() const {
    const auto& anchor = selectionAnchor_;
    const auto& focus = selectionFocus_;
    if (anchor.row < focus.row || (anchor.row == focus.row && anchor.cell <= focus.cell))
        return {anchor, focus};
    return {focus, anchor};
}

void ViewerView::beginSelection(int column, int row) {
    selecting_ = false;
    hasSelection_ = false;
    ensureVisible();
    if (visible_.empty()) return;
    selectionAnchor_ = pointAt(column, row);
    selectionFocus_ = selectionAnchor_;
    selecting_ = true;
}

void ViewerView::extendSelection(int column, int row) {
    ensureVisible();
    if (!selecting_ || visible_.empty()) return;
    selectionFocus_ = pointAt(column, row);
    hasSelection_ = selectionFocus_ != selectionAnchor_;
}

void ViewerView::endSelection(int column, int row) {
    ensureVisible();
    if (!selecting_) return;
    selectionFocus_ = pointAt(column, row);
    selecting_ = false;
    hasSelection_ = selectionFocus_ != selectionAnchor_;
}

void ViewerView::clearSelection() {
    selecting_ = false;
    hasSelection_ = false;
    selectionAnchor_ = {};
    selectionFocus_ = {};
}

std::string ViewerView::selectedText() const {
    if (!hasSelection_) return {};
    ensureVisible();
    if (visible_.empty()) return {};
    const auto [start, end] = selectionRange();
    const int first = std::max(0, start.row);
    const int last = std::min(end.row, static_cast<int>(visible_.size()) - 1);
    std::string result;
    bool firstRow = true;
    for (int index = first; index <= last; ++index) {
        if (!firstRow) result += '\n';
        firstRow = false;
        const auto& row = visible_[static_cast<std::size_t>(index)];
        const int left = index == start.row ? start.cell : 0;
        const int right = index == end.row ? end.cell : std::numeric_limits<int>::max();
        int column = 0;
        for (const auto& part : row.fragments) {
            for (const auto& glyph : displayGlyphs(part.text)) {
                const int cells = std::max(1, ftxui::string_width(glyph));
                if (column + cells - 1 >= left && column <= right) result += glyph;
                column += cells;
            }
        }
    }
    return result;
}

std::vector<PreviewRow> ViewerView::visibleRows() const {
    ensureVisible();
    std::vector<PreviewRow> result;
    result.reserve(visible_.size());
    SelectionPoint start;
    SelectionPoint end;
    const bool selecting = hasSelection_;
    if (selecting) {
        const auto range = selectionRange();
        start = range.first;
        end = range.second;
    }
    for (std::size_t i = 0; i < visible_.size(); ++i) {
        const auto& row = visible_[i];
        std::string prefix;
        if (width_ >= 3) prefix = row.line == target_ && row.firstGlyph == 0 ? "> " : "  ";
        if (numbers_) {
            const auto digits = std::to_string(lines_.size()).size();
            const auto number = row.firstGlyph == 0 ? std::to_string(row.line + 1) : "";
            prefix += std::string(digits - number.size(), ' ') + number + "  ";
        }
        prefix.resize(static_cast<std::size_t>(gutter()));
        PreviewRow content;
        append(content, prefix, false);
        int selectedLeft = 0;
        int selectedRight = -1;
        const int index = static_cast<int>(i);
        if (selecting && index >= start.row && index <= end.row) {
            selectedLeft = index == start.row ? start.cell : 0;
            selectedRight = index == end.row ? end.cell : std::numeric_limits<int>::max();
        }
        if (!selecting && wrap_ && horizontal_ == 0) {
            for (const auto& part : row.fragments)
                append(content, part.text, part.highlighted, part.selected);
        } else {
            for (const auto& part : slice(row.fragments, horizontal_, bodyWidth(), selectedLeft,
                                          selectedRight))
                append(content, part.text, part.highlighted, part.selected);
        }
        result.push_back(std::move(content));
    }
    return result;
}

std::string ViewerView::footer() const {
    const auto line = initial_ || lines_.empty() ? 0 : top_.line + 1;
    const auto state = "N:" + std::string(numbers_ ? "on" : "off") +
                       " W:" + (wrap_ ? "on" : "off");
    if (width_ < 65) return "Esc:뒤로 ↑↓ PgUp/Dn n:번호 w:wrap " + state;
    if (width_ < 125) return
        "Esc:뒤로 ↑↓/휠 PgUp/Dn Home/End ←→/^휠 드래그:선택 우클릭:복사 n:번호 w:wrap " + state;
    return "Esc:뒤로 ↑↓/휠:스크롤 PgUp/Dn Home/End ←→/Ctrl+휠:가로 "
           "드래그:선택 우클릭:복사 n:번호 w:wrap " + state + " | " +
           std::to_string(line) + "/" + std::to_string(lines_.size());
}

} // namespace mini_lexical_search::tui
