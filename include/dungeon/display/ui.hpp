#pragma once

#include "dungeon/display/buffer.hpp"
#include <string>

namespace dungeon::display::tui {

void label(FrameBuffer& buffer, std::string text, std::size_t x, std::size_t y);
void box(FrameBuffer& buffer, std::size_t w, std::size_t h, std::size_t x, std::size_t y);

}
