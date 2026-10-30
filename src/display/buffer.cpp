#include "dungeon/display/buffer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <utility>

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
    std::fputs("\033[H", stdout);

    std::swap(_back_buffer, _front_buffer);

    for (std::size_t y = 0; y < _height; ++y) {
        // fwrite prints an entire row efficiently in a single system call
        std::fwrite(&_front_buffer[y * _width], sizeof(char), _width, stdout);
        if (y + 1 < _height) {
            std::fputc('\n', stdout);
        }
    }

    std::fflush(stdout);

    std::fill(_back_buffer.begin(), _back_buffer.end(), ' ');
}

} // namespace dungeon::display
