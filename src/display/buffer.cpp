#include "dungeon/display/buffer.hpp"
#include <cstddef>

namespace dungeon::display {

void FrameBuffer::draw(std::size_t x, std::size_t y, char c)
{
    if (x >= _width || y >= _height) {
        return;
    }
    // Always draw to the back buffer
    _back_buffer[y * _width + x] = c;
}

void FrameBuffer::present()
{
    std::swap(_back_buffer, _front_buffer);

    // TODO: Flush _front_buffer characters to the actual terminal screen here,
    // or compare against previous states for differential rendering.

    std::copy(_front_buffer.begin(), _front_buffer.end(), _back_buffer.begin());
}

}

