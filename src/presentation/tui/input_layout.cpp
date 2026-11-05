#include "mini_lexical_search/presentation/tui/input_layout.hpp"

#include <algorithm>
#include "ftxui.hpp"
#include "mini_lexical_search/presentation/tui/result_view.hpp"

namespace mini_lexical_search::tui {

void reflowInput(std::string& content, int& cursor, int wrapWidth) {
    wrapWidth = std::max(1, wrapWidth);
    const int limit = std::clamp(cursor, 0, static_cast<int>(content.size()));
    const bool beforeBreak = limit < static_cast<int>(content.size()) &&
                             content[static_cast<std::size_t>(limit)] == '\n';
    int target = 0;
    std::string logical;
    logical.reserve(content.size());
    for (int i = 0; i < static_cast<int>(content.size()); ++i) {
        if (content[static_cast<std::size_t>(i)] == '\n') continue;
        logical.push_back(content[static_cast<std::size_t>(i)]);
        if (i < limit) ++target;
    }

    std::string wrapped;
    wrapped.reserve(logical.size());
    int lineWidth = 0;
    int seen = 0;
    int mapped = 0;
    bool mappedCursor = false;
    for (const auto& glyph : displayGlyphs(logical)) {
        const int width = ftxui::string_width(glyph);
        if (width > 0 && lineWidth > 0 && lineWidth + width > wrapWidth) {
            if (!mappedCursor && target == seen && beforeBreak) {
                mapped = static_cast<int>(wrapped.size());
                mappedCursor = true;
            }
            wrapped.push_back('\n');
            lineWidth = 0;
        }
        const int end = seen + static_cast<int>(glyph.size());
        if (!mappedCursor && target >= seen && target < end) {
            // Reflow adds only newlines; it must not snap an existing offset
            // to a different position inside or after a joined glyph.
            mapped = static_cast<int>(wrapped.size()) + target - seen;
            mappedCursor = true;
        }
        wrapped += glyph;
        seen = end;
        lineWidth += std::max(0, width);
    }
    if (!mappedCursor) mapped = static_cast<int>(wrapped.size());
    content = std::move(wrapped);
    cursor = mapped;
}

ftxui::Component searchInput(std::string& content, int& cursor, std::function<void()> onChange) {
    ftxui::InputOption option;
    option.multiline = true;
    option.cursor_position = &cursor;
    option.on_change = std::move(onChange);
    option.transform = [](ftxui::InputState state) {
        if (state.is_placeholder) state.element |= ftxui::dim;
        return state.element;
    };
    auto input = ftxui::Input(&content, option);
    return input | ftxui::CatchEvent([&content, &cursor](ftxui::Event event) {
        if (event == ftxui::Event::Backspace && cursor > 0 &&
            content[static_cast<std::size_t>(cursor - 1)] == '\n') --cursor;
        if (event == ftxui::Event::Delete && cursor < static_cast<int>(content.size()) &&
            content[static_cast<std::size_t>(cursor)] == '\n') ++cursor;
        return false;
    });
}

}  // namespace mini_lexical_search::tui
