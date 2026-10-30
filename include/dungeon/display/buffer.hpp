#pragma once

#include "dungeon/display/term.hpp"
#include <cstddef>
#include <vector>

namespace dungeon::display {

class FrameBuffer {
public:
    FrameBuffer() {
        int w = 0, h = 0;
        dungeon::display::tui::term_size(w, h);
        resize(static_cast<std::size_t>(w), static_cast<std::size_t>(h));
    }

    FrameBuffer(std::size_t width, std::size_t height) { resize(width, height); }

    [[nodiscard]] std::size_t width() const noexcept { return _width; }
    [[nodiscard]] std::size_t height() const noexcept { return _height; }

    void draw(std::size_t x, std::size_t y, char c);
    void present();

private:
    void resize(std::size_t width, std::size_t height) {
        _width  = width;
        _height = height;

        std::size_t total_size = _width * _height;
        _back_buffer.resize(total_size, ' ');
        _front_buffer.resize(total_size, ' ');
    }

    std::size_t _width;
    std::size_t _height;
    std::vector<char> _back_buffer;
    std::vector<char> _front_buffer;
};

}
