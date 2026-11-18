#include "mini_lexical_search/presentation/tui/result_view.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include "ftxui.hpp"
#include "mini_lexical_search/domain/query_processor.hpp"

namespace mini_lexical_search::tui {
namespace {
struct Glyph {
    std::string text;
    int width;
    bool hit;
};

std::vector<Glyph> glyphs(const std::string& source, const std::vector<std::string>& terms) {
    std::string text = source;
    for (char& c : text) if (static_cast<unsigned char>(c) < 32 || c == 127) c = ' ';
    SimpleTokenizer tokenizer;
    const auto tokens = tokenizer.tokenizeWithOffsets(text);
    std::vector<Glyph> result;
    std::size_t token = 0;
    std::size_t at = 0;
    for (const auto& value : displayGlyphs(text)) {
        const auto begin = text.find(value, at);
        if (begin == std::string::npos) continue;
        const auto end = begin + value.size();
        while (token < tokens.size() && tokens[token].end <= begin) ++token;
        const bool hit = token < tokens.size() && tokens[token].begin < end &&
                         tokens[token].end > begin &&
                         std::find(terms.begin(), terms.end(), tokens[token].normalized) != terms.end();
        result.push_back({value, std::max(1, ftxui::string_width(value)), hit});
        at = end;
    }
    return result;
}

void append(PreviewRow& row, const std::string& text, bool hit = false) {
    if (!row.empty() && row.back().highlighted == hit) row.back().text += text;
    else row.push_back({text, hit});
}

std::vector<PreviewRow> pack(const std::vector<Glyph>& units, std::size_t begin, int width,
                             int limit, bool prefix) {
    std::vector<std::vector<Glyph>> packed(1);
    int used = 0;
    if (prefix && width > 1) { packed.back().push_back({"…", 1, false}); used = 1; }
    std::size_t index = begin;
    for (; index < units.size(); ++index) {
        auto unit = units[index];
        if (unit.width > width) unit = {"�", 1, unit.hit};
        if (used + unit.width > width) {
            if (static_cast<int>(packed.size()) == limit) break;
            packed.emplace_back();
            used = 0;
        }
        packed.back().push_back(unit);
        used += unit.width;
    }
    if (index < units.size()) {
        while (used + 1 > width && !packed.back().empty()) {
            used -= packed.back().back().width;
            packed.back().pop_back();
        }
        packed.back().push_back({"…", 1, false});
    }
    std::vector<PreviewRow> rows;
    for (const auto& line : packed) {
        PreviewRow row;
        for (const auto& unit : line) append(row, unit.text, unit.hit);
        rows.push_back(std::move(row));
    }
    return rows;
}
}  // namespace

std::vector<std::string> displayGlyphs(const std::string& text) {
    std::vector<std::string> result;
    // Use the renderer's cluster boundaries for wrapping and highlighting too.
    for (const auto& glyph : ftxui::Utf8ToGlyphs(text)) {
        if (glyph.empty()) continue; // Full-width glyph's second cell.
        result.push_back(glyph);
    }
    return result;
}

std::vector<PreviewRow> preview(const std::string& text, const std::vector<std::string>& terms,
                                int width, int maxRows) {
    width = std::max(1, width);
    maxRows = std::clamp(maxRows, 1, 3);
    const auto units = glyphs(text, terms);
    // First check the actual wrapped height, including wide glyph boundaries.
    int lines = 1, used = 0;
    for (const auto& unit : units) {
        if (used && used + unit.width > width) { ++lines; used = 0; }
        used += std::min(width, unit.width);
    }
    if (lines <= maxRows) return pack(units, 0, width, maxRows, false);
    auto first = std::find_if(units.begin(), units.end(), [](const Glyph& glyph) { return glyph.hit; });
    std::size_t begin = static_cast<std::size_t>(first - units.begin());
    if (first == units.end()) begin = 0;
    int context = 0;
    while (begin > 0 && context + units[begin - 1].width <= width / 4) {
        context += units[--begin].width;
    }
    return pack(units, begin, width, maxRows, begin != 0);
}

std::string clipped(const std::string& text, int width) {
    std::string result;
    const auto rows = preview(text, {}, width, 1);
    for (const auto& part : rows.front()) result += part.text;
    return result;
}

void ResultView::home() { *this = ResultView{}; }

void ResultView::showResults(SearchPage result, const std::string& executedQuery, const std::string& time,
                             int scrollOffset) {
    mode = ViewMode::Results;
    page = std::move(result);
    query = executedQuery;
    elapsed = time;
    message_.clear();
    SimpleTokenizer tokenizer;
    terms_ = parseQuery(query, tokenizer).terms;
    rows_.clear(); items_.clear(); selected_ = -1; offset_ = std::max(0, scrollOffset);
}

void ResultView::showText(std::string message, const std::string& executedQuery) {
    mode = ViewMode::Text;
    page = {};
    query = executedQuery;
    elapsed.clear();
    message_ = std::move(message);
    rows_.clear(); items_.clear(); selected_ = -1; offset_ = 0;
    resultsFocused = false;
}

void ResultView::layout(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    const bool resized = width_ != width || height_ != height;
    if (!resized && !rows_.empty()) return;
    std::optional<ItemKey> selectedKey;
    if (selection()) selectedKey = selection()->key;
    std::optional<ItemKey> anchor;
    int anchorWithin = 0;
    if (offset_ < static_cast<int>(rows_.size()) && rows_[offset_].item >= 0) {
        const auto& item = items_[rows_[offset_].item];
        anchor = item.key;
        anchorWithin = offset_ - item.firstRow;
    }
    width_ = width; height_ = height;
    rows_.clear(); items_.clear();
    auto textRow = [&](std::string text, bool title = false) {
        rows_.push_back({{{clipped(text, width), false}}, -1, false, title});
    };
    auto addItem = [&](ItemKey key, std::vector<PreviewRow> content) {
        const int id = static_cast<int>(items_.size());
        items_.push_back({std::move(key), static_cast<int>(rows_.size()), static_cast<int>(content.size())});
        for (auto& row : content) rows_.push_back({std::move(row), id});
    };
    if (mode == ViewMode::Results) {
        if (page.files.empty()) textRow("No matches.");
        for (std::size_t f = 0; f < page.files.size(); ++f) {
            const auto& file = page.files[f];
            if (f) rows_.push_back({{}, -1, true});
            textRow("[" + file.relativePath + "] " +
                    (file.totalMatchingLines > 10 ? "10+" : std::to_string(file.totalMatchingLines)), true);
            for (const auto& line : file.lines) {
                const std::string prefix = std::to_string(line.number) + ": ";
                // Extremely narrow terminals still reserve a cell for the body.
                const int gutter = std::min(static_cast<int>(prefix.size()), std::max(0, width - 3));
                const int marker = width > 2 ? 2 : 0;
                auto content = preview(line.text, terms_, width - gutter - marker, std::min(3, height));
                for (std::size_t i = 0; i < content.size(); ++i) {
                    content[i].insert(content[i].begin(), {i ? std::string(gutter, ' ') : prefix.substr(0, gutter), false});
                    content[i].insert(content[i].begin(), {std::string(marker, ' '), false});
                }
                addItem({ItemKind::Line, file.relativePath, line.number}, std::move(content));
            }
            if (!file.warning.empty()) textRow("warning: " + file.warning);
            else if (file.lines.size() < file.totalMatchingLines) {
                addItem({ItemKind::More, file.relativePath, 0},
                        {{{clipped("[더보기 · " + std::to_string(file.totalMatchingLines - file.lines.size()) + "행 남음]", width), false}}});
            }
        }
        // Page controls live in a fixed footer, outside the scrollable rows.
        if (page.hasPrevious) items_.push_back({{ItemKind::Previous, {}, 0}, -1, 0});
        if (page.hasNext) items_.push_back({{ItemKind::Next, {}, 0}, -1, 0});
    } else if (mode == ViewMode::Text) {
        std::istringstream input(message_);
        std::string line;
        while (std::getline(input, line)) {
            // Command help and errors are not limited to a three-row snippet.
            const auto units = glyphs(line, {});
            std::size_t begin = 0;
            while (begin < units.size()) {
                PreviewRow row;
                int used = 0;
                do {
                    const auto& unit = units[begin];
                    if (used && used + unit.width > width) break;
                    append(row, unit.width > width ? "�" : unit.text);
                    used += std::min(unit.width, width);
                    ++begin;
                } while (begin < units.size());
                rows_.push_back({std::move(row)});
            }
            if (units.empty()) textRow("");
        }
    }
    selected_ = items_.empty() ? -1 : 0;
    if (selectedKey) {
        for (std::size_t i = 0; i < items_.size(); ++i) if (items_[i].key == *selectedKey) selected_ = static_cast<int>(i);
    }
    if (resized && anchor) {
        for (const auto& item : items_) if (item.key == *anchor) offset_ = item.firstRow + std::min(anchorWithin, item.rowCount - 1);
    }
    scroll(0);
}

const ResultItem* ResultView::selection() const {
    return selected_ >= 0 && selected_ < static_cast<int>(items_.size()) ? &items_[selected_] : nullptr;
}
void ResultView::reveal() {
    if (const auto* item = selection(); item && item->rowCount > 0) {
        if (item->firstRow < offset_) offset_ = item->firstRow;
        else if (item->firstRow + item->rowCount > offset_ + height_) offset_ = item->firstRow + item->rowCount - height_;
    }
    scroll(0);
}
void ResultView::select(int index, bool revealSelection) {
    if (items_.empty()) return;
    selected_ = std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
    if (revealSelection) reveal();
}
void ResultView::select(const ItemKey& key, bool revealSelection) {
    for (std::size_t i = 0; i < items_.size(); ++i)
        if (items_[i].key == key) { select(static_cast<int>(i), revealSelection); return; }
}
void ResultView::move(int delta) { select(selected_ + delta); }
void ResultView::scroll(int rows) {
    offset_ = std::clamp(offset_ + rows, 0, std::max(0, static_cast<int>(rows_.size()) - height_));
}
}  // namespace mini_lexical_search::tui
