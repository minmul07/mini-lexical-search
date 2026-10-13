#include "mini_lexical_search/presentation/tui/tui.hpp"

#include "ftxui.hpp"

namespace mini_lexical_search::tui {

void run() {
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto component = ftxui::Renderer([] {
        return ftxui::text("on development") | ftxui::center;
    });
    component |= ftxui::CatchEvent([&](ftxui::Event event) {
        if (event == ftxui::Event::Escape || event == ftxui::Event::Character('q')) {
            screen.Exit();
            return true;
        }
        return false;
    });
    screen.Loop(component);
}

}  // namespace mini_lexical_search::tui
