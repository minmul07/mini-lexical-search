#pragma once

#include <string>
#include <vector>
#include "mini_lexical_search/application/search_application.hpp"

namespace mini_lexical_search::tui {

struct Fragment {
    std::string text;
    bool highlighted = false;
    bool selected = false;
};
using PreviewRow = std::vector<Fragment>;
// 제어 문자는 공백으로 치환하며 원문의 토큰 경계로 강조한다.
std::vector<PreviewRow> preview(const std::string& text, const std::vector<std::string>& terms,
                                int width, int maxRows = 3);
std::vector<std::string> displayGlyphs(const std::string& text);
std::string clipped(const std::string& text, int width);
// 발췌·생략 없이 원문 전체를 셀 너비에 맞춰 줄바꿈한다.
std::vector<PreviewRow> wrapped(const std::string& text, const std::vector<std::string>& terms,
                                int width);

enum class ViewMode { Home, Results, Text };
enum class ItemKind { Line, More, Previous, Next };
struct ItemKey {
    ItemKind kind = ItemKind::Line;
    std::string path;
    std::uint32_t line = 0;
    bool operator==(const ItemKey&) const = default;
};
struct ResultItem {
    ItemKey key;
    int firstRow = 0;
    int rowCount = 1;
};
struct DisplayRow {
    PreviewRow fragments;
    int item = -1;
    bool separator = false;
    bool title = false;
};

// 터미널 이벤트와 독립적인 화면 상태. 레이아웃 단위는 터미널 셀과 표시 행이다.
class ResultView {
public:
    ViewMode mode = ViewMode::Home;
    SearchPage page;
    std::string query;
    std::string elapsed;
    bool resultsFocused = false;

    void home();
    void showResults(SearchPage result, const std::string& executedQuery, const std::string& time,
                     int scrollOffset = 0);
    void showText(std::string message, const std::string& executedQuery = {});
    void layout(int width, int height);
    void move(int delta);
    void select(int index, bool revealSelection = true);
    void select(const ItemKey& key, bool revealSelection = true);
    void scroll(int rows);
    const ResultItem* selection() const;
    const std::vector<ResultItem>& items() const { return items_; }
    const std::vector<DisplayRow>& rows() const { return rows_; }
    int selected() const { return selected_; }
    int offset() const { return offset_; }
    int height() const { return height_; }

private:
    void reveal();
    std::string message_;
    std::vector<std::string> terms_;
    std::vector<DisplayRow> rows_;
    std::vector<ResultItem> items_;
    int selected_ = -1;
    int offset_ = 0;
    int height_ = 1;
    int width_ = 1;
};

}  // namespace mini_lexical_search::tui
