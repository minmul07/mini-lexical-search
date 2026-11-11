#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ftxui.hpp"
#include "mini_lexical_search/presentation/tui/index_progress.hpp"

using namespace mini_lexical_search;
using namespace mini_lexical_search::tui;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string joined(const std::vector<std::string>& lines) {
    std::string result;
    for (const auto& line : lines) result += line + '\n';
    return result;
}

void testModel() {
    IndexProgressModel model;
    model.begin();
    auto display = model.display();
    require(display.active && !display.finished && !display.failed, "begin state");
    require(display.target > 0.0F && display.target < 0.1F, "begin target");
    float previous = display.target;

    IndexProgress event{};
    event.stage = IndexStage::MetadataStarted;
    model.apply(event);

    event.stage = IndexStage::MetadataProgress;
    event.completed = 1;
    event.total = 4;
    model.apply(event);
    display = model.display();
    require(display.target > previous && display.target < 0.25F, "metadata progress");
    require(joined(display.lines).find("1/4") != std::string::npos, "metadata count missing");
    previous = display.target;

    event = {};
    event.stage = IndexStage::MetadataCompleted;
    event.elapsedMicroseconds = 1234;
    event.metadata = {1, 2, 1, 0};
    model.apply(event);
    display = model.display();
    require(display.target >= previous && display.target <= 0.25F, "metadata completed target");
    require(joined(display.lines).find("updated file metadata (1.234 ms)") != std::string::npos,
            "metadata elapsed missing");
    require(joined(display.lines).find("1 not updated, 2 updated, 1 added, 0 deleted") !=
                std::string::npos,
            "metadata counters missing");
    previous = display.target;

    event = {};
    event.stage = IndexStage::TokenizationStarted;
    event.total = 3;
    model.apply(event);

    event = {};
    event.stage = IndexStage::TokenizationProgress;
    event.completed = 2;
    event.total = 3;
    model.apply(event);
    display = model.display();
    require(display.target > previous && display.target < 0.90F, "tokenization progress");
    require(joined(display.lines).find("2/3") != std::string::npos, "tokenization count missing");
    previous = display.target;

    event = {};
    event.stage = IndexStage::TokenizationCompleted;
    event.elapsedMicroseconds = 2500;
    event.tokens = {3, 12};
    model.apply(event);
    display = model.display();
    require(display.target >= previous && display.target <= 0.90F, "tokenization completed target");
    require(joined(display.lines).find("tokenized 3 documents (2.500 ms)") != std::string::npos,
            "tokenized summary missing");

    event = {};
    event.stage = IndexStage::SaveStarted;
    model.apply(event);
    event = {};
    event.stage = IndexStage::SaveCompleted;
    event.elapsedMicroseconds = 500;
    model.apply(event);
    display = model.display();
    require(display.target > 0.90F && display.target < 1.0F, "save target");
    require(joined(display.lines).find("saved index (0.500 ms)") != std::string::npos,
            "save summary missing");

    model.finish(IndexUpdate{});
    display = model.display();
    require(!display.active && display.finished && !display.failed && display.target == 1.0F,
            "finish state");

    model.begin();
    event = {};
    event.stage = IndexStage::MetadataStarted;
    model.apply(event);
    model.fail("boom");
    display = model.display();
    require(!display.active && display.finished && display.failed && display.error == "boom",
            "failure state");
    require(joined(display.lines).find("error: indexing failed: boom") != std::string::npos,
            "failure line missing");

    model.clear();
    display = model.display();
    require(!display.active && !display.finished && display.lines.empty() && display.target == 0.0F,
            "clear state");
}

void testGeometry() {
    for (int height : {6, 10, 24, 40}) {
        const auto geometry = homeGeometry(80, height, 1);
        require(geometry.fieldWidth == 78, "home field width");
        require(geometry.fieldLeft == 1, "home field left");
        require(geometry.boxHeight == 3, "home box height");
        require(geometry.detailTop == geometry.boxTop + geometry.boxHeight + 1,
                "detail offset");
        require(geometry.detailHeight >= 0, "negative detail height");
    }
    const auto first = homeGeometry(120, 30, 1);
    const auto second = homeGeometry(120, 30, 3);
    require(first.fieldWidth == 100 && first.fieldLeft == 10, "max home width");
    require(first.boxTop != second.boxTop, "input height must affect box position");
    require(first.detailHeight > 0, "detail area missing");
}

void testFill() {
    auto render = [](float progress, float opacity = 1.0F) {
        auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(8),
                                            ftxui::Dimension::Fixed(1));
        auto element = progressFill(ftxui::text("abcdefgh"), progress, opacity) |
                       ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8) |
                       ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 1);
        ftxui::Render(screen, element);
        return screen;
    };
    const auto fill = ftxui::Color::RGB(64, 64, 64);
    auto screen = render(0.0F);
    for (int x = 0; x < 8; ++x) {
        require(screen.CellAt(x, 0).background_color == ftxui::Color::Default, "unexpected fill");
    }
    screen = render(0.5F);
    for (int x = 0; x < 4; ++x) {
        require(screen.CellAt(x, 0).background_color == fill,
                "left half not filled: got " +
                    screen.CellAt(x, 0).background_color.Print(false) + " want " +
                    fill.Print(false));
    }
    for (int x = 4; x < 8; ++x) {
        require(screen.CellAt(x, 0).background_color == ftxui::Color::Default,
                "right half filled");
    }
    require(screen.at(0, 0) == "a" && screen.at(7, 0) == "h", "fill changed text");
    screen = render(1.0F);
    for (int x = 0; x < 8; ++x) {
        require(screen.CellAt(x, 0).background_color == fill, "full fill missing");
    }

    const auto half = ftxui::Color::Interpolate(0.5F, fill, ftxui::Color::RGB(0, 0, 0));
    screen = render(1.0F, 0.5F);
    for (int x = 0; x < 8; ++x) {
        require(screen.CellAt(x, 0).background_color == half, "half-opacity fill missing");
    }
    screen = render(1.0F, 0.0F);
    for (int x = 0; x < 8; ++x) {
        require(screen.CellAt(x, 0).background_color == ftxui::Color::Default,
                "transparent fill missing");
    }
}

}  // namespace

int main() {
    try {
        testModel();
        testGeometry();
        testFill();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
