#include "dungeon/display/ui.hpp"

namespace dungeon::display::tui {

void label(FrameBuffer& buffer, std::string text, std::size_t x, std::size_t y)
{
    for (size_t i = 0; i < text.size(); i++)
        buffer.draw(x + i, y, text.at(i));
}

void box(FrameBuffer& buffer, std::size_t w, std::size_t h, std::size_t x, std::size_t y)
{

}

}

