#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
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
    std::vector<PreviewRow> visibleRows() const;
    std::string footer() const;
    const std::string& path() const { return path_; }
    const std::string& notice() const { return notice_; }
    bool numbers() const { return numbers_; }
    bool wrap() const { return wrap_; }
    int offset() const { return offset_; }
    int horizontalOffset() const { return horizontal_; }
    int height() const { return height_; }
    const std::vector<ViewerRow>& rows() const { return rows_; }

private:
    void rebuild();
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
    bool numbers_ = false;
    bool wrap_ = true;
    bool initial_ = true;
};

} // namespace mini_lexical_search::tui
