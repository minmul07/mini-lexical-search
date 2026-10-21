#include "mini_lexical_search/presentation/tui/tui.hpp"

#include "mini_lexical_search/presentation/cli/cli.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>

#include "ftxui.hpp"

namespace mini_lexical_search::tui {
namespace {

constexpr int kMaxInnerWidth = 100;
constexpr int kBorderColumns = 2;

std::size_t utf8Next(std::string_view text, std::size_t index) {
    if (index >= text.size()) {
        return index;
    }
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    std::size_t length = 1;
    if ((lead & 0x80) == 0) {
        length = 1;
    } else if ((lead & 0xE0) == 0xC0) {
        length = 2;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
    }
    if (index + length > text.size()) {
        return index + 1;
    }
    return index + length;
}

int logicalOffset(const std::string& wrapped, int cursor) {
    const int limit = std::max(0, std::min(cursor, static_cast<int>(wrapped.size())));
    int offset = 0;
    for (int i = 0; i < limit; ++i) {
        if (wrapped[static_cast<std::size_t>(i)] != '\n') {
            ++offset;
        }
    }
    return offset;
}

int lineCount(const std::string& content) {
    int lines = 1;
    for (char ch : content) {
        if (ch == '\n') {
            ++lines;
        }
    }
    return lines;
}

int wrapWidthFor(const ftxui::Screen& screen) {
    const int terminalWidth = std::max(screen.dimx(), ftxui::Terminal::Size().dimx);
    return std::max(1, std::min(kMaxInnerWidth, terminalWidth - kBorderColumns));
}

void reflow(std::string& content, int& cursor, int wrapWidth) {
    wrapWidth = std::max(1, wrapWidth);
    const int target = logicalOffset(content, cursor);

    std::string logical;
    logical.reserve(content.size());
    for (char ch : content) {
        if (ch != '\n') {
            logical.push_back(ch);
        }
    }

    std::string wrapped;
    wrapped.reserve(logical.size());
    int lineWidth = 0;
    int seen = 0;
    int mapped = 0;
    bool mappedCursor = false;

    auto placeCursor = [&](int at) {
        if (!mappedCursor && seen >= target) {
            mapped = at;
            mappedCursor = true;
        }
    };

    std::size_t index = 0;
    while (index < logical.size()) {
        const std::size_t next = utf8Next(logical, index);
        const std::string_view glyph(logical.data() + index, next - index);
        const int width = ftxui::string_width(glyph);
        if (width > 0 && lineWidth > 0 && lineWidth + width > wrapWidth) {
            wrapped.push_back('\n');
            lineWidth = 0;
        }
        placeCursor(static_cast<int>(wrapped.size()));
        wrapped.append(logical, index, next - index);
        seen += static_cast<int>(next - index);
        lineWidth += std::max(0, width);
        index = next;
    }
    placeCursor(static_cast<int>(wrapped.size()));
    if (!mappedCursor) {
        mapped = static_cast<int>(wrapped.size());
    }

    content = std::move(wrapped);
    cursor = mapped;
}

std::string wrapLine(std::string_view line, int wrapWidth) {
    wrapWidth = std::max(1, wrapWidth);
    std::string wrapped;
    int lineWidth = 0;
    std::size_t index = 0;
    while (index < line.size()) {
        const std::size_t next = utf8Next(line, index);
        const std::string_view glyph(line.data() + index, next - index);
        const int width = ftxui::string_width(glyph);
        if (width > 0 && lineWidth > 0 && lineWidth + width > wrapWidth) {
            wrapped.push_back('\n');
            lineWidth = 0;
        }
        wrapped.append(line.data() + index, next - index);
        lineWidth += std::max(0, width);
        index = next;
    }
    return wrapped;
}

std::string wrapOutput(std::string_view text, int wrapWidth) {
    if (!text.empty() && text.back() == '\n') {
        text.remove_suffix(1);
    }
    if (text.empty()) {
        return {};
    }
    std::string wrapped;
    std::size_t start = 0;
    bool hasLine = false;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        const auto lineEnd = end == std::string_view::npos ? text.size() : end;
        if (hasLine) {
            wrapped.push_back('\n');
        }
        wrapped += wrapLine(text.substr(start, lineEnd - start), wrapWidth);
        hasLine = true;
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
        if (start == text.size()) {
            wrapped.push_back('\n');
            break;
        }
    }
    return wrapped;
}

std::string logicalText(const std::string& content) {
    std::string line;
    line.reserve(content.size());
    for (char ch : content) {
        if (ch != '\n') {
            line.push_back(ch);
        }
    }
    return line;
}

}  // namespace

void run(const char* program, SearchApplication& application) {
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    std::string content;
    int cursor = 0;
    std::string output;
    if (!application.ready()) {
        output = "error: cannot load " + application.snapshotPath().string() + ": " +
                 application.loadError() + "\n";
    }

    ftxui::InputOption option;
    option.multiline = true;
    option.cursor_position = &cursor;
    option.transform = [](ftxui::InputState state) {
        if (state.is_placeholder) {
            state.element |= ftxui::dim;
        }
        return state.element;
    };
    option.on_change = [&] {
        reflow(content, cursor, wrapWidthFor(screen));
    };

    auto input = ftxui::Input(&content, option);
    auto component = ftxui::Renderer(input, [&] {
        const int width = wrapWidthFor(screen);
        reflow(content, cursor, width);
        auto field = input->Render() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width) |
                     ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, lineCount(content)) |
                     ftxui::borderStyled(ftxui::Color::White);
        ftxui::Elements column{std::move(field)};
        const std::string wrapped = wrapOutput(output, width);
        if (!wrapped.empty()) {
            column.push_back(ftxui::text(wrapped) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width) |
                             ftxui::hcenter);
        }
        return ftxui::vbox(std::move(column)) | ftxui::center;
    });
    component |= ftxui::CatchEvent([&](ftxui::Event event) {
        if (event == ftxui::Event::Escape) {
            screen.Exit();
            return true;
        }
        if (event == ftxui::Event::Return) {
            const std::string line = logicalText(content);
            content.clear();
            cursor = 0;
            std::ostringstream commandOutput;
            const auto action = cli::handleLine(program, application, line, commandOutput);
            if (action == cli::LineAction::Quit) {
                screen.Exit();
            } else if (action == cli::LineAction::Ran) {
                output = commandOutput.str();
            }
            return true;
        }
        return false;
    });
    screen.Loop(component);
}

}  // namespace mini_lexical_search::tui
