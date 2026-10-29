#pragma once

#include "dungeon/core/term.hpp"
#include <cstddef>
#include <vector>
#include <algorithm>

namespace dungeon::display {

class FrameBuffer {
public:
    FrameBuffer() {
        int w = 0, h = 0;
        dungeon::core::term_size(w, h);
        _width  = static_cast<std::size_t>(w);
        _height = static_cast<std::size_t>(h);

        std::size_t total_size = _width * _height;
        _back_buffer.resize(total_size, ' ');
        _front_buffer.resize(total_size, ' ');
    }

    void draw(std::size_t x, std::size_t y, char c);
    void present();

private:
    std::size_t _width;
    std::size_t _height;
    std::vector<char> _back_buffer;
    std::vector<char> _front_buffer;
};

}
