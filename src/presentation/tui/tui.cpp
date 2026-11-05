#include "mini_lexical_search/presentation/tui/tui.hpp"

#include "mini_lexical_search/presentation/cli/cli.hpp"
#include "mini_lexical_search/application/working_job.hpp"
#include "mini_lexical_search/presentation/tui/input_layout.hpp"
#include "mini_lexical_search/presentation/tui/result_view.hpp"

#include <algorithm>
#include <exception>
#include <sstream>
#include <string>
#include <string_view>

#include "ftxui.hpp"

namespace mini_lexical_search::tui {
namespace {

constexpr int kMaxInnerWidth = 100;
constexpr int kBorderColumns = 2;

int lineCount(const std::string& content) {
    int lines = 1;
    for (char ch : content) {
        if (ch == '\n') {
            ++lines;
        }
    }
    return lines;
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
    ResultView view;
    std::string sessionQuery;
    std::string sessionElapsed;
    int areaWidth = 1;
    int fieldWidth = 1;
    int inputHeight = 1;
    ftxui::Box outputBox;
    ftxui::Box inputBox;
    ftxui::Box footerBox;
    struct Hit { ftxui::Box box; int item; };
    std::vector<Hit> hits;
    auto geometry = [&] {
        const auto size = ftxui::Terminal::Size();
        areaWidth = std::max(1, size.dimx - 4);
        fieldWidth = view.mode == ViewMode::Home
                         ? std::max(1, std::min(kMaxInnerWidth, size.dimx - kBorderColumns))
                         : std::max(1, areaWidth - kBorderColumns);
        reflowInput(content, cursor, fieldWidth);
        inputHeight = std::min({3, lineCount(content), std::max(1, size.dimy - 6)});
        if (view.mode != ViewMode::Home) {
            const int queryRows = view.query.empty() ? 0 : 1;
            const int pageRows = view.mode == ViewMode::Results ? 1 : 0;
            view.layout(areaWidth, std::max(1, size.dimy - inputHeight - 2 - queryRows - pageRows - 1));
        }
    };
    bool showPrompt = !application.ready();
    std::string promptError;
    bool mouseMode = false;
    int mouseIndex = -1;
    ftxui::Box resetBox;
    ftxui::Box quitBox;

    auto input = searchInput(content, cursor, geometry);
    auto resultFocus = ftxui::Button("", [] {});
    auto focusGroup = ftxui::Container::Vertical({input, resultFocus});
    auto setFocus = [&](bool results) {
        view.resultsFocused = results;
        (results ? resultFocus : input)->TakeFocus();
    };
    ftxui::ButtonOption action;
    action.transform = [](const ftxui::EntryState& state) { return ftxui::text(state.label); };
    auto resetIndex = ftxui::Button(
        "reset index",
        [&] {
            try {
                application.rebuildIndex("./documents");
                promptError.clear();
                showPrompt = false;
            } catch (const std::exception& error) {
                promptError = error.what();
            }
        },
        action);
    auto quit = ftxui::Button("quit", [&] { screen.Exit(); }, action);
    auto choices = ftxui::Container::Horizontal({resetIndex, quit});
    auto paintChoice = [&](ftxui::Component choice, int index, ftxui::Box& box) {
        auto element = choice->Render() | ftxui::reflect(box);
        const bool highlighted = mouseMode ? mouseIndex == index : choice->Focused();
        if (highlighted) {
            element |= ftxui::inverted;
        }
        return element;
    };
    auto prompt = ftxui::Renderer(choices, [&] {
        ftxui::Elements message{ftxui::text("Recreate the index?")};
        if (!promptError.empty()) {
            message.push_back(ftxui::text(promptError));
        }
        auto dialog = ftxui::vbox({
            ftxui::text("Failed to load index") | ftxui::bold,
            ftxui::text(""),
            ftxui::vbox(std::move(message)),
            ftxui::text(""),
            ftxui::hbox({
                ftxui::filler(),
                paintChoice(resetIndex, 0, resetBox),
                ftxui::text("  "),
                paintChoice(quit, 1, quitBox),
            }),
        });
        return ftxui::hbox({
                   ftxui::text(" "),
                   ftxui::vbox({ftxui::text(""), std::move(dialog), ftxui::text("")}),
                   ftxui::text(" "),
               }) |
               ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 52) | ftxui::border;
    });
    prompt |= ftxui::CatchEvent([&](ftxui::Event event) {
        if (event == ftxui::Event::Escape) return true;
        if (event.is_mouse()) {
            if (event.mouse().motion == ftxui::Mouse::Moved) {
                mouseMode = true;
                if (resetBox.Contain(event.mouse().x, event.mouse().y)) {
                    mouseIndex = 0;
                } else if (quitBox.Contain(event.mouse().x, event.mouse().y)) {
                    mouseIndex = 1;
                } else {
                    mouseIndex = -1;
                }
            }
            return false;
        }
        if (mouseMode && mouseIndex >= 0) {
            (mouseIndex == 0 ? resetIndex : quit)->TakeFocus();
        }
        mouseMode = false;
        return false;
    });
    auto applyFeedback = [&](cli::CommandFeedback& feedback, const std::string& text) {
        if (feedback.newSearch) {
            sessionQuery = feedback.query;
            sessionElapsed = feedback.elapsed;
        }
        if (feedback.display == cli::Display::Results && feedback.page) {
            view.showResults(std::move(*feedback.page), sessionQuery, sessionElapsed);
        } else if (feedback.display == cli::Display::Text) {
            view.showText(text, feedback.newSearch ? feedback.query : "");
        }
        geometry();
    };
    auto activate = [&] {
        if (!view.selection()) return;
        const auto key = view.selection()->key;
        if (key.kind == ItemKind::Line) return;
        std::optional<ItemKey> firstNew;
        if (key.kind == ItemKind::More) {
            for (const auto& file : view.page.files) if (file.relativePath == key.path) {
                // The next matching row number is known after expanding the page.
                const auto count = file.lines.size();
                std::ostringstream output;
                cli::CommandFeedback feedback;
                cli::navigate(application, cli::Navigation::More, key.path, output, &feedback);
                if (feedback.page) {
                    for (const auto& expanded : feedback.page->files) {
                        if (expanded.relativePath == key.path && expanded.lines.size() > count)
                            firstNew = ItemKey{ItemKind::Line, key.path, expanded.lines[count].number};
                    }
                }
                applyFeedback(feedback, output.str());
                break;
            }
        } else {
            std::ostringstream output;
            cli::CommandFeedback feedback;
            cli::navigate(application, key.kind == ItemKind::Next ? cli::Navigation::Next
                                                                 : cli::Navigation::Previous,
                          "", output, &feedback);
            applyFeedback(feedback, output.str());
        }
        setFocus(view.mode == ViewMode::Results);
        if (firstNew) view.select(*firstNew);
        else if (view.mode == ViewMode::Results) view.select(0);
    };
    auto component = ftxui::Renderer(focusGroup, [&] {
        if (showPrompt) return ftxui::filler();
        geometry();
        auto field = input->Render() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, fieldWidth) |
                     ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, inputHeight) |
                     ftxui::borderStyled(ftxui::Color::White) | ftxui::reflect(inputBox);
        if (view.mode == ViewMode::Home) {
            hits.clear();
            return ftxui::dbox({
                field | ftxui::center,
                ftxui::vbox({ftxui::text(clipped(workingJob().label(), ftxui::Terminal::Size().dimx)) |
                                 ftxui::align_right, ftxui::filler()}),
            });
        }
        ftxui::Elements column{std::move(field)};
        if (!view.query.empty()) {
            column.push_back(ftxui::text(clipped("검색: " + view.query +
                                    (view.elapsed.empty() ? "" : "  (" + view.elapsed + " ms)"), areaWidth)));
        }
        const auto& rows = view.rows();
        const int end = std::min(static_cast<int>(rows.size()), view.offset() + view.height());
        const int visibleCount = end - view.offset();
        const int controls = view.mode == ViewMode::Results ? view.page.hasPrevious + view.page.hasNext : 0;
        hits.resize(static_cast<std::size_t>(visibleCount + controls));
        ftxui::Elements visible;
        for (int i = view.offset(); i < end; ++i) {
            const auto& row = rows[i];
            ftxui::Element element;
            if (row.separator) element = ftxui::separator();
            else {
                ftxui::Elements fragments;
                for (std::size_t j = 0; j < row.fragments.size(); ++j) {
                    const auto& fragment = row.fragments[j];
                    std::string text = fragment.text;
                    const bool selected = view.resultsFocused && row.item >= 0 && row.item == view.selected();
                    if (selected && j == 0 && row.item >= 0 &&
                        view.items()[row.item].key.kind == ItemKind::Line &&
                        view.items()[row.item].firstRow == i && text == "  ") text = "> ";
                    auto part = ftxui::text(text);
                    if (fragment.highlighted) part |= ftxui::bgcolor(ftxui::Color::RGB(60, 70, 80)) |
                                                               ftxui::color(ftxui::Color::White);
                    if (selected) part |= ftxui::underlined;
                    fragments.push_back(std::move(part));
                }
                element = ftxui::hbox(std::move(fragments));
                if (row.title) element |= ftxui::bold;
            }
            auto& hit = hits[static_cast<std::size_t>(i - view.offset())];
            hit.item = row.item;
            visible.push_back(element | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, areaWidth) |
                              ftxui::reflect(hit.box));
        }
        column.push_back(ftxui::vbox(std::move(visible)) |
                         ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, view.height()) |
                         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, areaWidth) | ftxui::reflect(outputBox));
        if (view.mode == ViewMode::Results) {
            int hitIndex = visibleCount;
            auto control = [&](ItemKind kind, const std::string& label, bool enabled) {
                auto element = ftxui::text(label);
                if (!enabled) return element | ftxui::dim;
                for (std::size_t i = 0; i < view.items().size(); ++i) {
                    if (view.items()[i].key.kind != kind) continue;
                    auto& hit = hits[hitIndex++];
                    hit.item = static_cast<int>(i);
                    element |= ftxui::reflect(hit.box);
                    if (view.resultsFocused && view.selected() == static_cast<int>(i))
                        element = element | ftxui::underlined | ftxui::inverted;
                    break;
                }
                return element;
            };
            const bool compact = areaWidth < 26;
            auto footer = ftxui::hbox({control(ItemKind::Previous, compact ? "<" : "[이전]", view.page.hasPrevious),
                                      ftxui::text((compact ? " " : "  Page ") + std::to_string(view.page.number) +
                                                  (compact ? " " : "  ")),
                                      control(ItemKind::Next, compact ? ">" : "[다음]", view.page.hasNext)});
            column.push_back(footer | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, areaWidth) |
                             ftxui::reflect(footerBox));
        }
        column.push_back(ftxui::text(clipped("↑↓: 검색창/결과  Space/Enter: 실행  Esc: 홈  | " + workingJob().label(), areaWidth)) |
                         ftxui::dim);
        return ftxui::hbox({ftxui::text("  "), ftxui::vbox(std::move(column)), ftxui::filler()});
    });
    component |= ftxui::CatchEvent([&](ftxui::Event event) {
        if (event == ftxui::Event::CtrlC) return false;
        geometry();
        if (event == ftxui::Event::Escape) {
            if (view.mode != ViewMode::Home) {
                application.clearSearch();
                view.home();
                sessionQuery.clear();
                sessionElapsed.clear();
                content.clear();
                cursor = 0;
                setFocus(false);
            }
            return true;
        }
        if (event.is_mouse()) {
            const auto& mouse = event.mouse();
            if (inputBox.Contain(mouse.x, mouse.y) && mouse.button == ftxui::Mouse::Left &&
                mouse.motion == ftxui::Mouse::Pressed) {
                setFocus(false);
                return false;
            }
            if (view.mode != ViewMode::Home && (outputBox.Contain(mouse.x, mouse.y) ||
                 (view.mode == ViewMode::Results && footerBox.Contain(mouse.x, mouse.y)))) {
                if (mouse.button == ftxui::Mouse::WheelUp || mouse.button == ftxui::Mouse::WheelDown) {
                    view.scroll(mouse.button == ftxui::Mouse::WheelDown ? 3 : -3);
                    return true;
                }
                if (mouse.button == ftxui::Mouse::Left && mouse.motion == ftxui::Mouse::Pressed) {
                    for (const auto& hit : hits) if (hit.item >= 0 && hit.box.Contain(mouse.x, mouse.y)) {
                        const int item = hit.item;
                        setFocus(true);
                        view.select(item);
                        activate();
                        break;
                    }
                    return true;
                }
            }
            return false;
        }
        if (event == ftxui::Event::Tab || event == ftxui::Event::TabReverse) {
            // 컨테이너의 기본 Tab 포커스 이동도 막는다.
            return true;
        }
        if (event == ftxui::Event::ArrowUp || event == ftxui::Event::ArrowDown) {
            if (view.resultsFocused) {
                if (event == ftxui::Event::ArrowUp && view.selected() <= 0) setFocus(false);
                else view.move(event == ftxui::Event::ArrowDown ? 1 : -1);
            } else {
                const auto position = static_cast<std::size_t>(std::clamp(cursor, 0, static_cast<int>(content.size())));
                const bool lastInputLine = content.find('\n', position) == std::string::npos;
                if (event == ftxui::Event::ArrowDown && lastInputLine && !view.items().empty()) {
                    setFocus(true);
                    view.select(0);
                } else {
                    input->OnEvent(event);
                }
            }
            // 입력 경계에서 컨테이너가 보이지 않는 결과 컴포넌트로 이동하지 않게 한다.
            return true;
        }
        if (event == ftxui::Event::PageUp || event == ftxui::Event::PageDown) {
            view.scroll(event == ftxui::Event::PageDown ? view.height() : -view.height());
            return true;
        }
        if (view.resultsFocused) {
            if (event == ftxui::Event::Home) view.select(0);
            else if (event == ftxui::Event::End) view.select(static_cast<int>(view.items().size()) - 1);
            else if (event == ftxui::Event::Character(' ') || event == ftxui::Event::Return) activate();
            return true;
        }
        if (event == ftxui::Event::Return) {
            const std::string line = logicalText(content);
            std::ostringstream output;
            cli::CommandFeedback feedback;
            const auto action = cli::handleLine(program, application, line, output, cli::OutputMode::Tui, &feedback);
            if (action == cli::LineAction::Quit) screen.Exit();
            else if (action == cli::LineAction::Ran) {
                if (feedback.newSearch) {
                    content = feedback.query;
                    cursor = static_cast<int>(content.size());
                } else { content.clear(); cursor = 0; }
                applyFeedback(feedback, output.str());
                setFocus(false);
            }
            return true;
        }
        return false;
    });
    auto root = ftxui::Modal(component, prompt, &showPrompt);
    screen.Loop(root);
}

}  // namespace mini_lexical_search::tui
