#include "mini_lexical_search/presentation/tui/tui.hpp"

#include "mini_lexical_search/presentation/cli/cli.hpp"
#include "mini_lexical_search/application/working_job.hpp"
#include "mini_lexical_search/presentation/tui/clipboard.hpp"
#include "mini_lexical_search/presentation/tui/index_progress.hpp"
#include "mini_lexical_search/presentation/tui/input_layout.hpp"
#include "mini_lexical_search/presentation/tui/result_view.hpp"
#include "mini_lexical_search/presentation/tui/viewer_view.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include "ftxui.hpp"

namespace mini_lexical_search::tui {
namespace {

constexpr int kBorderColumns = 2;
constexpr auto kMoreRevealDuration = std::chrono::milliseconds(200);
constexpr auto kDoubleClickInterval = std::chrono::milliseconds(700);
constexpr auto kCopyNoticeSlideDuration = std::chrono::milliseconds(300);
constexpr auto kCopyNoticeHoldDuration = std::chrono::seconds(3);
using Clock = std::chrono::steady_clock;

// 더보기로 추가된 행을 ease-out으로 순차 노출하기 위한 상태다.
struct MoreReveal {
    bool active = false;
    ItemKey firstKey;
    Clock::time_point started{};
};

// 글리프 경계를 지키면서 [left, left + width) 셀을 잘라 고정 폭으로 만든다.
std::string clipCells(const std::string& text, int left, int width) {
    std::string result;
    int column = 0;
    int used = 0;
    const int end = left + width;
    for (const auto& glyph : displayGlyphs(text)) {
        const int cells = std::max(1, ftxui::string_width(glyph));
        const int begin = std::max(left, column);
        const int stop = std::min(end, column + cells);
        if (begin < stop) {
            if (begin == column && stop == column + cells) result += glyph;
            else result += std::string(stop - begin, ' ');
            used += stop - begin;
        }
        column += cells;
        if (column >= end) break;
    }
    if (used < width) result += std::string(width - used, ' ');
    return result;
}

// 우측 상단에 겹쳐 그릴 복사 알림 박스다. 오른쪽으로 밀린 만큼만 잘라 그린다.
ftxui::Element copyNotice(const std::string& message, int boxWidth, int frameWidth,
                          int offset) {
    const std::string label = " " + message + " ";
    std::string horizontal;
    for (int i = 0; i < boxWidth - 2; ++i) horizontal += "─";
    const std::string top = "┌" + horizontal + "┐";
    const std::string middle = "│" + label + "│";
    const std::string bottom = "└" + horizontal + "┘";
    const int visible = std::clamp(frameWidth - offset, 0, boxWidth);
    auto spacer = [](int cells) {
        return ftxui::filler() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, cells);
    };
    auto line = [&](const std::string& text) {
        ftxui::Elements parts{ftxui::filler()};
        if (offset > 0) parts.push_back(spacer(offset));
        if (visible > 0) {
            parts.push_back(ftxui::text(clipCells(text, 0, visible)) |
                            ftxui::color(ftxui::Color::GreenLight) | ftxui::bold |
                            ftxui::bgcolor(ftxui::Color::RGB(0, 0, 0)));
        }
        const int right = frameWidth - offset - visible;
        if (right > 0) parts.push_back(spacer(right));
        return ftxui::hbox(std::move(parts));
    };
    return ftxui::vbox({
        line(top),
        line(middle),
        line(bottom),
        ftxui::filler(),
    });
}

// 전체 화면을 같은 배경색으로 칠한다.
ftxui::Element blackBackground(ftxui::Element element) {
    return ftxui::bgcolor(ftxui::Color::RGB(0, 0, 0), std::move(element));
}

bool isQuitCommand(const std::string& line) {
    const auto first = line.find_first_not_of(" \t\r\n\v\f");
    if (first == std::string::npos) return false;
    const auto last = line.find_last_not_of(" \t\r\n\v\f");
    const auto command = line.substr(first, last - first + 1);
    return command == "/exit" || command == "/quit";
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
    // 인덱싱 중에는 종료를 미룰 수 있도록 Ctrl+C 처리를 직접 맡는다.
    screen.ForceHandleCtrlC(false);
    std::string content;
    int cursor = 0;
    ResultView view;
    std::optional<ViewerView> viewer;
    std::string sessionQuery;
    std::string sessionElapsed;
    int areaWidth = 1;
    int fieldWidth = 1;
    int inputHeight = 1;
    ftxui::Box outputBox;
    ftxui::Box inputBox;
    ftxui::Box footerBox;
    struct Hit { ftxui::Box box; int item = -1; };
    std::vector<Hit> hits;
    HomeGeometry homeLayout;
    auto geometry = [&] {
        const auto size = ftxui::Terminal::Size();
        areaWidth = std::max(1, size.dimx - 4);
        if (viewer) {
            viewer->layout(areaWidth, std::max(1, size.dimy - 2 - !viewer->notice().empty()));
            return;
        }
        fieldWidth = view.mode == ViewMode::Home ? homeFieldWidth(size.dimx)
                                                 : std::max(1, areaWidth - kBorderColumns);
        reflowInput(content, cursor, fieldWidth);
        inputHeight = std::min({3, lineCount(content), std::max(1, size.dimy - 6)});
        if (view.mode == ViewMode::Home) {
            homeLayout = homeGeometry(size.dimx, size.dimy, inputHeight);
        } else {
            const int queryRows = view.query.empty() ? 0 : 1;
            const int pageRows = view.mode == ViewMode::Results ? 1 : 0;
            view.layout(areaWidth, std::max(1, size.dimy - inputHeight - 2 - queryRows - pageRows - 1));
        }
    };
    bool showPrompt = !application.ready();
    std::string promptError;
    bool indexRunning = false;
    bool exitWhenIndexed = false;
    bool fillFinishing = false;
    bool fillFading = false;
    float fill = 0.0F;
    float fillOpacity = 1.0F;
    Clock::time_point fillFadeStarted{};
    Clock::time_point lastFrame = Clock::now();
    Clock::time_point copyNoticeStarted{};
    constexpr auto kFillFadeDuration = std::chrono::milliseconds(750);
    MoreReveal moreReveal;
    IndexProgressModel indexModel;
    std::jthread indexWorker;
    bool mouseMode = false;
    int mouseIndex = -1;
    std::optional<ItemKey> lastClickKey;
    Clock::time_point lastClickAt{};
    ftxui::Box resetBox;
    ftxui::Box quitBox;

    auto input = searchInput(content, cursor, geometry);
    auto resultFocus = ftxui::Button("", [] {});
    auto focusGroup = ftxui::Container::Vertical({input, resultFocus});
    auto setFocus = [&](bool results) {
        view.resultsFocused = results;
        (results ? resultFocus : input)->TakeFocus();
    };
    auto startIndex = [&](bool rebuild, const std::string& directory) {
        if (indexRunning) return;
        indexRunning = true;
        exitWhenIndexed = false;
        fillFinishing = false;
        fillFading = false;
        fill = 0.0F;
        fillOpacity = 1.0F;
        fillFadeStarted = {};
        moreReveal.active = false;
        view.home();
        sessionQuery.clear();
        sessionElapsed.clear();
        content.clear();
        cursor = 0;
        setFocus(false);
        indexModel.begin();
        geometry();
        indexWorker = std::jthread([&, rebuild, directory](std::stop_token) {
            auto lastPost = Clock::now();
            const auto notify = [&](const IndexProgress& event) {
                indexModel.apply(event);
                const auto now = Clock::now();
                if (event.stage == IndexStage::MetadataProgress ||
                    event.stage == IndexStage::TokenizationProgress) {
                    if (now - lastPost < std::chrono::milliseconds(33)) return;
                }
                lastPost = now;
                screen.PostEvent(ftxui::Event::Custom);
            };
            try {
                const auto result = rebuild ? application.rebuildIndex(directory, notify)
                                            : application.index(directory, notify);
                indexModel.finish(result);
            } catch (const std::exception& error) {
                indexModel.fail(error.what());
            } catch (...) {
                indexModel.fail("unknown error");
            }
            screen.PostEvent(ftxui::Event::Custom);
        });
    };
    ftxui::ButtonOption action;
    action.transform = [](const ftxui::EntryState& state) { return ftxui::text(state.label); };
    auto resetIndex = ftxui::Button(
        "reset index",
        [&] {
            showPrompt = false;
            promptError.clear();
            startIndex(true, "./documents");
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
        return blackBackground(ftxui::hbox({
                   ftxui::text(" "),
                   ftxui::vbox({ftxui::text(""), std::move(dialog), ftxui::text("")}),
                   ftxui::text(" "),
               }) |
               ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 52) | ftxui::border);
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
    auto applyFeedback = [&](cli::CommandFeedback& feedback, const std::string& text,
                             int scrollOffset = 0) {
        moreReveal.active = false;
        lastClickKey.reset();
        if (!indexRunning) indexModel.clear();
        if (feedback.newSearch) {
            sessionQuery = feedback.query;
            sessionElapsed = feedback.elapsed;
        }
        if (feedback.display == cli::Display::Results && feedback.page) {
            view.showResults(std::move(*feedback.page), sessionQuery, sessionElapsed, scrollOffset);
        } else if (feedback.display == cli::Display::Text) {
            view.showText(text, feedback.newSearch ? feedback.query : "");
        }
        geometry();
    };
    auto activate = [&] {
        lastClickKey.reset();
        if (!view.selection()) return;
        const auto key = view.selection()->key;
        if (key.kind == ItemKind::Line) {
            viewer.emplace();
            try {
                viewer->open(key.path, application.readFile(key.path), key.line, view.query);
            } catch (const std::exception& error) {
                viewer->showError(key.path, error.what());
            }
            geometry();
            return;
        }
        std::optional<ItemKey> firstNew;
        if (key.kind == ItemKind::More) {
            const int scrollOffset = view.offset();
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
                // Keep the rows above the button in place while the file expands.
                applyFeedback(feedback, output.str(), scrollOffset);
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
        if (firstNew) {
            view.select(*firstNew, false);
            moreReveal.active = true;
            moreReveal.firstKey = *firstNew;
            moreReveal.started = Clock::now();
        } else if (view.mode == ViewMode::Results) {
            view.select(0, key.kind != ItemKind::More);
        }
    };
    auto component = ftxui::Renderer(focusGroup, [&] {
        if (showPrompt) return blackBackground(ftxui::filler());
        geometry();
        if (viewer) {
            ftxui::Elements column{
                ftxui::text(clipped("[" + viewer->path() + "]", areaWidth)) | ftxui::bold};
            if (!viewer->notice().empty())
                column.push_back(ftxui::text(clipped(viewer->notice(), areaWidth)) | ftxui::dim);
            ftxui::Elements visible;
            for (const auto& row : viewer->visibleRows()) {
                ftxui::Elements fragments;
                for (const auto& fragment : row) {
                    auto part = ftxui::text(fragment.text);
                    if (fragment.highlighted)
                        part |= ftxui::bgcolor(ftxui::Color::RGB(60, 70, 80)) |
                                ftxui::color(ftxui::Color::White);
                    if (fragment.selected) part |= ftxui::inverted;
                    fragments.push_back(std::move(part));
                }
                visible.push_back(ftxui::hbox(std::move(fragments)));
            }
            column.push_back(ftxui::vbox(std::move(visible)) |
                             ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, viewer->height()) |
                             ftxui::size(ftxui::WIDTH, ftxui::EQUAL, areaWidth));
            column.push_back(ftxui::text(clipped(viewer->footer(), areaWidth)) | ftxui::dim);
            auto page = ftxui::hbox({ftxui::text("  "),
                ftxui::vbox(std::move(column)), ftxui::filler()});
            if (!viewer->status().empty()) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    Clock::now() - copyNoticeStarted).count();
                const auto slide = std::chrono::duration_cast<std::chrono::milliseconds>(
                    kCopyNoticeSlideDuration).count();
                const auto hold = std::chrono::duration_cast<std::chrono::milliseconds>(
                    kCopyNoticeHoldDuration).count();
                if (elapsed >= slide + hold + slide) {
                    viewer->clearStatus();
                } else {
                    const bool slideOut = elapsed >= slide + hold;
                    const auto phase = slideOut ? elapsed - slide - hold : elapsed;
                    const float progress = std::clamp(
                        static_cast<float>(phase) / static_cast<float>(slide), 0.0F, 1.0F);
                    const float eased = ftxui::animation::easing::QuadraticOut(progress);
                    const int width =
                        ftxui::string_width(" " + viewer->status() + " ") + 2;
                    const int frameWidth = width + 2;
                    const int offset = static_cast<int>(std::lround(
                        static_cast<float>(frameWidth) * (slideOut ? eased : 1.0F - eased)));
                    page = ftxui::dbox({std::move(page),
                                        copyNotice(viewer->status(), width, frameWidth,
                                                   offset)});
                    ftxui::animation::RequestAnimationFrame();
                }
            }
            return blackBackground(std::move(page));
        }
        const auto now = Clock::now();
        const auto delta = std::clamp(std::chrono::duration<float>(now - lastFrame).count(),
                                      0.0F, 0.1F);
        lastFrame = now;
        int revealFrom = 0;
        int revealTo = 0;
        if (moreReveal.active) {
            const auto& items = view.items();
            int first = -1;
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (items[i].key == moreReveal.firstKey) {
                    first = static_cast<int>(i);
                    break;
                }
            }
            int totalItems = 0;
            if (first >= 0) {
                for (std::size_t i = static_cast<std::size_t>(first); i < items.size(); ++i) {
                    const auto& item = items[i];
                    if (item.key.path != moreReveal.firstKey.path) break;
                    if (item.key.kind != ItemKind::Line && item.key.kind != ItemKind::More) break;
                    ++totalItems;
                }
            }
            if (totalItems == 0) {
                moreReveal.active = false;
            } else {
                const float elapsed =
                    std::chrono::duration<float>(now - moreReveal.started).count();
                const float duration = std::chrono::duration<float>(kMoreRevealDuration).count();
                const float progress = std::clamp(elapsed / duration, 0.0F, 1.0F);
                const int startRow = items[static_cast<std::size_t>(first)].firstRow;
                const auto& last = items[static_cast<std::size_t>(first + totalItems - 1)];
                const int endRow = last.firstRow + last.rowCount;
                const int totalRows = endRow - startRow;
                int shownRows = totalRows;
                if (progress < 1.0F) {
                    // Blend the decelerating curve with linear motion so the last
                    // row still appears near the end of the 200ms window.
                    const auto eased =
                        0.5F * ftxui::animation::easing::QuadraticOut(progress) + 0.5F * progress;
                    shownRows = std::clamp(
                        static_cast<int>(std::lround(static_cast<float>(totalRows) * eased)), 1,
                        totalRows);
                    ftxui::animation::RequestAnimationFrame();
                } else {
                    moreReveal.active = false;
                }
                revealFrom = startRow + shownRows;
                revealTo = endRow;
            }
        }
        const auto indexDisplay = indexModel.display();
        if (indexRunning) {
            if (fill < indexDisplay.target) {
                fill = std::min(indexDisplay.target, fill + std::max(0.015F, delta * 1.5F));
            } else if (fill > indexDisplay.target) {
                fill = std::max(indexDisplay.target, fill - delta * 2.0F);
            }
            if (fill < indexDisplay.target - 0.001F) ftxui::animation::RequestAnimationFrame();
        } else if (fillFinishing) {
            if (fill < 1.0F) {
                fill = std::min(1.0F, fill + std::max(0.02F, delta * 2.0F));
                ftxui::animation::RequestAnimationFrame();
            } else {
                fill = 1.0F;
                fillFinishing = false;
                fillFading = true;
                fillFadeStarted = now;
                fillOpacity = 1.0F;
                ftxui::animation::RequestAnimationFrame();
            }
        } else if (fillFading) {
            const auto elapsed = std::chrono::duration<float>(now - fillFadeStarted).count();
            const auto duration = std::chrono::duration<float>(kFillFadeDuration).count();
            const auto progress = std::clamp(elapsed / duration, 0.0F, 1.0F);
            fillOpacity = 1.0F - progress;
            if (progress >= 1.0F) {
                fillFading = false;
                fill = 0.0F;
                fillOpacity = 1.0F;
            } else {
                ftxui::animation::RequestAnimationFrame();
            }
        }
        auto inner = input->Render() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, fieldWidth) |
                     ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, inputHeight);
        if ((indexRunning || fillFinishing || fillFading) && fill > 0.0F) {
            inner = progressFill(std::move(inner), fill, fillOpacity);
        }
        auto field = std::move(inner) | ftxui::borderStyled(ftxui::Color::White) |
                     ftxui::reflect(inputBox);
        if (view.mode == ViewMode::Home) {
            hits.clear();
            ftxui::Elements details;
            if (homeLayout.detailHeight > 0 && !indexDisplay.lines.empty()) {
                const int count = std::min(homeLayout.detailHeight,
                                           static_cast<int>(indexDisplay.lines.size()));
                const int start = static_cast<int>(indexDisplay.lines.size()) - count;
                for (int i = 0; i < count; ++i) {
                    details.push_back(ftxui::text(
                        clipped(indexDisplay.lines[static_cast<std::size_t>(start + i)],
                                homeLayout.fieldWidth)));
                }
            }
            auto fieldLayer = ftxui::vbox({
                ftxui::text("") | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, homeLayout.boxTop),
                field | ftxui::hcenter,
                ftxui::filler(),
            });
            auto detailLayer = ftxui::hbox({
                ftxui::text("") | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, homeLayout.fieldLeft),
                ftxui::vbox({
                    ftxui::text("") | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, homeLayout.detailTop),
                    ftxui::vbox(std::move(details)) | ftxui::dim,
                    ftxui::filler(),
                }) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, homeLayout.fieldWidth),
                ftxui::filler(),
            });
            return blackBackground(ftxui::dbox({
                std::move(fieldLayer),
                std::move(detailLayer),
                ftxui::vbox({ftxui::text(clipped(workingJob().label(), ftxui::Terminal::Size().dimx)) |
                                 ftxui::align_right, ftxui::filler()}),
            }));
        }
        ftxui::Elements column{std::move(field)};
        if (!view.query.empty()) {
            column.push_back(ftxui::text(clipped("검색: " + view.query +
                                    (view.elapsed.empty() ? "" : "  (" + view.elapsed + " ms)"), areaWidth)));
        }
        const auto& rows = view.rows();
        const int controls = view.mode == ViewMode::Results ? view.page.hasPrevious + view.page.hasNext : 0;
        hits.assign(static_cast<std::size_t>(view.height() + controls), Hit{});
        int hitCount = 0;
        ftxui::Elements visible;
        // Hidden expansion rows do not consume viewport slots, so following
        // items slide down as the new rows take their space.
        for (int i = view.offset();
             i < static_cast<int>(rows.size()) && hitCount < view.height(); ++i) {
            if (i >= revealFrom && i < revealTo) continue;
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
            auto& hit = hits[static_cast<std::size_t>(hitCount++)];
            hit.item = row.item;
            visible.push_back(element | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, areaWidth) |
                              ftxui::reflect(hit.box));
        }
        column.push_back(ftxui::vbox(std::move(visible)) |
                         ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, view.height()) |
                         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, areaWidth) | ftxui::reflect(outputBox));
        if (view.mode == ViewMode::Results) {
            int hitIndex = hitCount;
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
        return blackBackground(
            ftxui::hbox({ftxui::text("  "), ftxui::vbox(std::move(column)), ftxui::filler()}));
    });
    component |= ftxui::CatchEvent([&](ftxui::Event event) {
        if (event == ftxui::Event::Custom) {
            if (indexRunning) {
                const auto status = indexModel.display();
                if (status.finished) {
                    indexRunning = false;
                    if (status.failed) {
                        fillFinishing = false;
                        fillFading = false;
                        fill = 0.0F;
                        fillOpacity = 1.0F;
                        if (!application.ready()) {
                            promptError = status.error;
                            showPrompt = true;
                        }
                    } else {
                        fillFinishing = true;
                        fillFading = false;
                        fillOpacity = 1.0F;
                        fillFadeStarted = {};
                    }
                    geometry();
                    if (exitWhenIndexed) screen.Exit();
                }
            }
            return true;
        }
        // Mouse release/move must not cut the animation that a click just started.
        bool interruptsReveal = true;
        if (event.is_mouse()) {
            const auto& mouse = event.mouse();
            interruptsReveal =
                (mouse.button == ftxui::Mouse::Left && mouse.motion == ftxui::Mouse::Pressed) ||
                mouse.button == ftxui::Mouse::WheelUp || mouse.button == ftxui::Mouse::WheelDown;
        }
        if (moreReveal.active && interruptsReveal) moreReveal.active = false;
        if (event == ftxui::Event::CtrlC) {
            if (indexRunning) {
                exitWhenIndexed = true;
            } else {
                screen.Exit();
            }
            return true;
        }
        geometry();
        if (viewer) {
            if (event == ftxui::Event::Escape) {
                viewer.reset();
                geometry();
                return true;
            }
            if (event.is_mouse()) {
                const auto& mouse = event.mouse();
                const int bodyTop = 1 + (viewer->notice().empty() ? 0 : 1);
                const int column = mouse.x - 2;
                const int row = mouse.y - bodyTop;
                if (mouse.button == ftxui::Mouse::Right &&
                    mouse.motion == ftxui::Mouse::Pressed) {
                    const auto text = viewer->selectedText();
                    if (!text.empty()) {
                        if (copyToClipboard(text)) {
                            viewer->clearSelection();
                            viewer->setStatus("Copied to clipboard.");
                        } else {
                            viewer->setStatus("Copy failed.");
                        }
                        copyNoticeStarted = Clock::now();
                    }
                    return true;
                }
                if (mouse.motion == ftxui::Mouse::Moved) {
                    viewer->extendSelection(column, row);
                    return true;
                }
                if (mouse.motion == ftxui::Mouse::Released) {
                    viewer->endSelection(column, row);
                    return true;
                }
                if (mouse.button == ftxui::Mouse::Left &&
                    mouse.motion == ftxui::Mouse::Pressed) {
                    if (column >= 0 && column < areaWidth &&
                        row >= 0 && row < viewer->height()) {
                        viewer->beginSelection(column, row);
                    } else {
                        viewer->clearSelection();
                    }
                    return true;
                }
                viewer->handleEvent(event);
                return true;
            }
            viewer->handleEvent(event);
            return true;
        }
        if (event == ftxui::Event::Escape) {
            if (view.mode != ViewMode::Home) {
                application.clearSearch();
                moreReveal.active = false;
                lastClickKey.reset();
                view.home();
                indexModel.clear();
                fill = 0.0F;
                fillFinishing = false;
                fillFading = false;
                fillOpacity = 1.0F;
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
                    bool hitFound = false;
                    for (const auto& hit : hits) if (hit.item >= 0 && hit.box.Contain(mouse.x, mouse.y)) {
                        const int item = hit.item;
                        setFocus(true);
                        view.select(item);
                        hitFound = true;
                        if (!view.selection()) break;
                        const auto key = view.selection()->key;
                        const auto now = Clock::now();
                        const bool doubleClick = lastClickKey && *lastClickKey == key &&
                                                 now - lastClickAt <= kDoubleClickInterval;
                        lastClickKey = key;
                        lastClickAt = now;
                        if (key.kind == ItemKind::Line) {
                            // 결과 행은 한 번의 클릭으로 선택만 하고, 더블클릭해야 뷰어로 들어간다.
                            if (doubleClick) activate();
                        } else {
                            // 더보기·페이지 버튼은 기존처럼 한 번의 클릭으로 실행한다.
                            activate();
                        }
                        break;
                    }
                    if (!hitFound) lastClickKey.reset();
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
            if (indexRunning) {
                if (isQuitCommand(line)) {
                    exitWhenIndexed = true;
                    content.clear();
                    cursor = 0;
                }
                return true;
            }
            std::ostringstream output;
            cli::CommandFeedback feedback;
            const auto action = cli::handleLine(
                program, application, line, output, cli::OutputMode::Tui, &feedback,
                [&](const std::string& directory) { startIndex(false, directory); });
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
