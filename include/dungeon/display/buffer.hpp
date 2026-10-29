#pragma once

#include "dungeon/core/term.hpp"
#include <cstddef>
#include <vector>
namespace dungeon::display {

class FrameBuffer {
public:
    FrameBuffer() {
        int w = 0, h = 0;
        dungeon::core::term_size(w, h);
        _width  = static_cast<std::size_t>(w);
        _height = static_cast<std::size_t>(h);
        _frame.clear();
    }

    void draw(std::size_t x, std::size_t y, char c);

private:
    std::size_t _width;
    std::size_t _height;
    std::vector<char> _frame;
};

}
