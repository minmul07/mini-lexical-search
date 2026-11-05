#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "ftxui.hpp"
#include "mini_lexical_search/presentation/tui/input_layout.hpp"
#include "mini_lexical_search/presentation/tui/result_view.hpp"

using namespace mini_lexical_search::tui;

void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

std::string logical(std::string text) {
    std::erase(text, '\n');
    return text;
}

int logicalCursor(const std::string& text, int cursor) {
    return cursor - static_cast<int>(std::count(text.begin(), text.begin() + cursor, '\n'));
}

struct Input {
    std::string content;
    int cursor = 0;
    int width = 12;
    ftxui::Component component;

    Input() {
        component = searchInput(content, cursor, [&] { reflowInput(content, cursor, width); });
        component->TakeFocus();
    }

    ftxui::Screen render() {
        reflowInput(content, cursor, width);
        auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width + 2),
                                           ftxui::Dimension::Fixed(5));
        auto element = component->Render() | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 3) | ftxui::border;
        ftxui::Render(screen, element);
        return screen;
    }

    void send(ftxui::Event event) {
        component->OnEvent(event);
        render();
    }
};

int main() {
    try {
        const std::vector<std::string> emoji = {"👩‍💻", "👨‍👩‍👧", "🧑‍🍳", "👍🏽", "🇰🇷", "❤️", "1️⃣"};
        for (const auto& cluster : emoji) {
            require(ftxui::string_width(cluster) == 2, "emoji must occupy two terminal cells");
            require(ftxui::Utf8ToGlyphs(cluster) == std::vector<std::string>{cluster, ""},
                    "renderer split a cluster across separate cells");
            require(ftxui::CellToGlyphIndex(cluster + cluster) == std::vector<int>{0, 0, 1, 1},
                    "cell-to-glyph mapping disagrees with rendered width");
            require(clipped(cluster, 2) == cluster, "exact-width emoji was truncated");
            Input input;
            input.send(ftxui::Event::Character("alpha" + cluster));
            input.send(ftxui::Event::ArrowLeft);
            require(input.cursor == 5, "left cursor trapped inside emoji after reflow");
            input.send(ftxui::Event::ArrowLeft);
            require(input.cursor == 4, "left cursor did not cross emoji");
            input.send(ftxui::Event::ArrowRight);
            input.send(ftxui::Event::ArrowRight);
            require(input.cursor == static_cast<int>(input.content.size()), "right cursor split emoji");
            input.send(ftxui::Event::Backspace);
            require(input.content == "alpha" && input.cursor == 5, "backspace left a partial emoji");
            input.send(ftxui::Event::Character(cluster));
            input.send(ftxui::Event::ArrowLeft);
            input.send(ftxui::Event::Delete);
            require(input.content == "alpha", "delete left a partial emoji");

            for (int width : {1, 4, 6, 12, 40}) {
                const std::string source = "alpha" + cluster + "끝";
                for (int at = 0; at <= static_cast<int>(source.size()); ++at) {
                    auto content = source;
                    int cursor = at;
                    reflowInput(content, cursor, width);
                    require(logical(content) == source && logicalCursor(content, cursor) == at,
                            "reflow changed text or logical cursor offset");
                    const auto wrapped = content;
                    const auto wrappedCursor = cursor;
                    reflowInput(content, cursor, width);
                    require(content == wrapped && cursor == wrappedCursor, "reflow was not stable");
                }
            }

            Input positioned;
            positioned.width = 8;
            positioned.send(ftxui::Event::Character("a" + cluster + "b"));
            const auto screen = positioned.render();
            require(screen.cursor().x == 5 && screen.cursor().y == 1,
                    "visible cursor disagrees with emoji cell width");
            require(screen.CellAt(9, 1).character == "│", "emoji overwrote the input border");
            ftxui::Mouse mouse;
            mouse.button = ftxui::Mouse::Left;
            mouse.motion = ftxui::Mouse::Pressed;
            mouse.x = 3; // The second cell of the emoji in the input's first row.
            mouse.y = 1;
            positioned.send(ftxui::Event::Mouse("", mouse));
            require(positioned.cursor == 1 + static_cast<int>(cluster.size()),
                    "mouse click left the cursor inside an emoji");
            positioned.send(ftxui::Event::Backspace);
            require(positioned.content == "ab", "mouse editing split an emoji");

            auto rendered = ftxui::Screen::Create(ftxui::Dimension::Fixed(8), ftxui::Dimension::Fixed(3));
            const std::string repeated = cluster + cluster + cluster;
            ftxui::Render(rendered, ftxui::text(repeated) | ftxui::border);
            require(rendered.ToString().find("│" + repeated + "│") != std::string::npos,
                    "serialized emoji row has extra cells or a displaced border");
        }

        Input repeated;
        repeated.width = 10;
        const std::string source = "민물 🧑‍🍳🪀🧑‍🍳🪀🧑‍🍳🪀🧑‍🍳🪀";
        repeated.send(ftxui::Event::Character(source));
        for (int width : {10, 6, 20, 1, 12}) {
            repeated.width = width;
            repeated.render();
            const auto steps = displayGlyphs(repeated.content).size();
            for (std::size_t count = 0; count <= steps; ++count)
                repeated.send(ftxui::Event::ArrowLeft);
            require(repeated.cursor == 0 && logical(repeated.content) == source,
                    "repeated emoji or resize trapped the cursor");
            repeated.send(ftxui::Event::End);
            repeated.send(ftxui::Event::Character("x"));
            require(logical(repeated.content) == source + "x", "editing after resize changed the query");
            repeated.send(ftxui::Event::Backspace);
        }
        repeated.send(ftxui::Event::End);
        const auto glyphCount = displayGlyphs(source).size();
        for (std::size_t count = 0; count < glyphCount; ++count)
            repeated.send(ftxui::Event::Backspace);
        require(repeated.content.empty(), "backspace was trapped at a soft line break");
        repeated.send(ftxui::Event::Character(source));
        repeated.send(ftxui::Event::Home);
        for (std::size_t count = 0; count < glyphCount; ++count)
            repeated.send(ftxui::Event::Delete);
        require(repeated.content.empty(), "delete was trapped at a soft line break");

        Input typed;
        typed.width = 6;
        for (const auto& codepoint : {"a", "b", "👩", "‍", "💻", "👍", "🏽", "🇰", "🇷"})
            typed.send(ftxui::Event::Character(codepoint));
        require(logical(typed.content) == "ab👩‍💻👍🏽🇰🇷", "incremental emoji input changed the query");
        for (int count = 0; count < 5; ++count) typed.send(ftxui::Event::Backspace);
        require(typed.content.empty(), "incremental emoji input left fragments after deletion");

        Input vertical;
        vertical.width = 4;
        vertical.send(ftxui::Event::Character("ab👩‍💻cd👍🏽ef"));
        vertical.send(ftxui::Event::ArrowUp);
        require(logicalCursor(vertical.content, vertical.cursor) == 15,
                "up cursor did not retain terminal column across emoji");
        vertical.send(ftxui::Event::ArrowDown);
        require(logicalCursor(vertical.content, vertical.cursor) == 25,
                "down cursor did not retain terminal column across emoji");

        require(ftxui::Utf8ToGlyphs("한\u0301") == std::vector<std::string>{"한\u0301", ""} &&
                clipped("한\u0301", 2) == "한\u0301", "combining mark detached from its wide base");
        require(ftxui::string_width("e\u0301") == 1, "narrow combining sequence changed width");

        auto narrow = ftxui::Screen::Create(ftxui::Dimension::Fixed(3), ftxui::Dimension::Fixed(3));
        ftxui::Render(narrow, (ftxui::text("👩‍💻") | ftxui::frame) | ftxui::border);
        require(narrow.CellAt(1, 1).character == "�" && narrow.CellAt(2, 1).character == "│" &&
                narrow.ToString().find("│�│") != std::string::npos,
                "partially clipped emoji painted over the narrow input border");

        Input boundary;
        boundary.width = 2;
        boundary.send(ftxui::Event::Character("abc"));
        boundary.send(ftxui::Event::ArrowLeft);
        boundary.send(ftxui::Event::ArrowLeft);
        require(boundary.cursor == 2, "reflow pushed cursor back across a visual line break");
        boundary.send(ftxui::Event::ArrowLeft);
        require(boundary.cursor == 1, "left cursor could not cross a visual line break");
        std::cout << "tui_input_test passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
