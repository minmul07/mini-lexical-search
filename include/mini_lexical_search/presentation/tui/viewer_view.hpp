#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "mini_lexical_search/presentation/tui/result_view.hpp"

namespace ftxui { struct Event; }

namespace mini_lexical_search::tui {

struct ViewerRow {
    PreviewRow fragments;
    std::size_t line = 0; // 0-based 원본 줄
    std::size_t firstGlyph = 0;
};

// 터미널 렌더링과 독립적인 읽기 전용 뷰어. 원문은 열린 동안만 소유하고,
// 표시 행은 현재 뷰포트와 스크롤에 필요한 원본 줄만 그때그때 계산한다.
class ViewerView {
public:
    void open(std::string path, std::vector<std::string> lines, std::uint32_t target,
              const std::string& query);
    void showError(std::string path, std::string error);
    void layout(int width, int height);
    void scroll(int rows);
    void scrollHorizontal(int cells);
    void toggleNumbers();
    void toggleWrap();
    // Esc·Ctrl+C는 호출자가 처리한다. 나머지 입력은 뷰어에서만 사용한다.
    void handleEvent(ftxui::Event event);
    // 본문 영역 좌표(열은 뷰어 전체 폭, 행은 본문 표시 행 기준)로 선택한다.
    void beginSelection(int column, int row);
    void extendSelection(int column, int row);
    void endSelection(int column, int row);
    void clearSelection();
    bool hasSelection() const { return hasSelection_; }
    std::string selectedText() const;
    void setStatus(std::string status) { status_ = std::move(status); }
    void clearStatus() { status_.clear(); }
    std::vector<PreviewRow> visibleRows() const;
    std::string footer() const;
    const std::string& path() const { return path_; }
    const std::string& notice() const { return notice_; }
    const std::string& status() const { return status_; }
    bool numbers() const { return numbers_; }
    bool wrap() const { return wrap_; }
    int horizontalOffset() const { return horizontal_; }
    int height() const { return height_; }
    // 현재 화면 첫 표시 행이 가리키는 원본 줄·글리프 위치.
    std::size_t topLine() const { return top_.line; }
    std::size_t topGlyph() const { return top_.glyph; }

private:
    struct Position {
        std::size_t line = 0;
        std::size_t glyph = 0; // 표시 행이 시작하는 글리프 인덱스
        bool operator==(const Position&) const = default;
        bool operator<(const Position& other) const {
            return line != other.line ? line < other.line : glyph < other.glyph;
        }
    };
    struct LineLayout {
        std::vector<std::size_t> starts;   // 표시 행별 시작 글리프. 항상 계산한다.
        std::vector<PreviewRow> segments;  // 표시용 조각. materialize일 때만 채운다.
    };
    struct SelectionPoint {
        int row = 0; // 현재 뷰포트 기준 표시 행
        int cell = 0;
        bool operator==(const SelectionPoint&) const = default;
    };
    // 표시 행 경계만 필요하면 materialize=false로 측정한다.
    LineLayout scanLine(std::size_t line, bool materialize) const;
    std::size_t segmentIndex(const LineLayout& layout, std::size_t glyph) const;
    Position segmentPosition(std::size_t line, std::size_t glyph) const;
    Position initialTop() const;
    Position lastRow() const;
    Position retreat(Position from, int rows) const;
    Position advance(Position from, int rows) const;
    void ensureMaxTop();
    void ensureMaxLineWidth();
    void resetLayout();
    void ensureVisible() const;
    void jumpHome();
    void jumpEnd();
    SelectionPoint pointAt(int column, int row) const;
    std::pair<SelectionPoint, SelectionPoint> selectionRange() const;
    int gutter() const;
    int bodyWidth() const;
    std::string path_;
    std::string notice_;
    std::vector<std::string> lines_;
    std::unordered_set<std::string> terms_;
    Position top_;
    Position resetTop_;
    Position maxTop_;
    mutable std::vector<ViewerRow> visible_;
    std::size_t target_ = 0;
    int width_ = 1;
    int height_ = 1;
    int horizontal_ = 0;
    int maxLineWidth_ = 0;
    SelectionPoint selectionAnchor_;
    SelectionPoint selectionFocus_;
    std::string status_;
    mutable bool visibleValid_ = false;
    bool maxTopKnown_ = false;
    bool maxLineWidthKnown_ = false;
    bool selecting_ = false;
    bool hasSelection_ = false;
    bool numbers_ = true;
    bool wrap_ = true;
    bool initial_ = true;
};

} // namespace mini_lexical_search::tui
