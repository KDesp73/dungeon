#include "dungeon/display/buffer.hpp"
#include <cstddef>

namespace dungeon::display {

void FrameBuffer::draw(std::size_t x, std::size_t y, char c)
{
    if (x >= _width || y >= _height) {
         throw std::out_of_range("FrameBuffer::draw: Coordinates out of bounds");
    }

    auto index = y * _width + x;
    _frame[index] = c;
}

}

