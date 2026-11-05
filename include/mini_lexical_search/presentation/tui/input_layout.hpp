#pragma once

#include <functional>
#include <string>
#include "ftxui.hpp"

namespace mini_lexical_search::tui {

// Insert visual line breaks while retaining the cursor's logical UTF-8 offset.
void reflowInput(std::string& content, int& cursor, int wrapWidth);

// Treat inserted line breaks as layout, including when deleting across them.
ftxui::Component searchInput(std::string& content, int& cursor, std::function<void()> onChange);

}  // namespace mini_lexical_search::tui
