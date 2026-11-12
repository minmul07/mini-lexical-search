#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
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

// 터미널 렌더링과 독립적인 읽기 전용 뷰어. 원문은 열린 동안만 소유한다.
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
    int offset() const { return offset_; }
    int horizontalOffset() const { return horizontal_; }
    int height() const { return height_; }
    const std::vector<ViewerRow>& rows() const { return rows_; }

private:
    struct SelectionPoint {
        int row = 0;
        int cell = 0;
        bool operator==(const SelectionPoint&) const = default;
    };
    void rebuild();
    SelectionPoint pointAt(int column, int row) const;
    std::pair<SelectionPoint, SelectionPoint> selectionRange() const;
    int gutter() const;
    int bodyWidth() const;
    std::string path_;
    std::string notice_;
    std::vector<std::string> lines_;
    std::vector<std::string> terms_;
    std::vector<ViewerRow> rows_;
    std::size_t target_ = 0;
    int width_ = 1;
    int height_ = 1;
    int offset_ = 0;
    int maxOffset_ = 0;
    int horizontal_ = 0;
    int maxLineWidth_ = 0;
    SelectionPoint selectionAnchor_;
    SelectionPoint selectionFocus_;
    std::string status_;
    bool selecting_ = false;
    bool hasSelection_ = false;
    bool numbers_ = true;
    bool wrap_ = true;
    bool initial_ = true;
};

} // namespace mini_lexical_search::tui
