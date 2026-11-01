#pragma once

#include "dungeon/display/style.hpp"
#include "dungeon/display/term.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dungeon::display {

/// One character cell in a framebuffer.
struct Cell {
    char32_t  glyph = U' ';
    CellStyle style{};

    friend constexpr bool operator==(const Cell&, const Cell&) = default;
};

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

    /// Returns the ASCII character currently staged at (@p x, @p y).
    ///
    /// Non-ASCII glyphs are reported as `?`. Out-of-range coordinates return a
    /// space. Use `glyph_at()` or `cell_at()` for Unicode-aware access.
    [[nodiscard]] char at(std::size_t x, std::size_t y) const noexcept;

    /// Returns the Unicode glyph currently staged at (@p x, @p y).
    ///
    /// Out-of-range coordinates return a space.
    [[nodiscard]] char32_t glyph_at(std::size_t x, std::size_t y) const noexcept;

    /// Returns the full cell currently staged at (@p x, @p y).
    ///
    /// Out-of-range coordinates return a default blank cell.
    [[nodiscard]] const Cell& cell_at(std::size_t x, std::size_t y) const noexcept;

    void draw(std::size_t x, std::size_t y, char c);
    void draw(std::size_t x, std::size_t y, char32_t c);
    void draw(std::size_t x, std::size_t y, char c, CellStyle style);
    void draw(std::size_t x, std::size_t y, char32_t c, CellStyle style);

    /// Renders the staged frame as ANSI escape sequences and UTF-8 text.
    ///
    /// The returned string starts with the cursor-home sequence and uses ANSI
    /// SGR codes for colors and text attributes. It is safe to write directly to
    /// a terminal and useful for tests.
    [[nodiscard]] std::string render() const;

    void present();

  private:
    void resize(std::size_t width, std::size_t height) {
        _width  = width;
        _height = height;

        const std::size_t total_size = _width * _height;
        _back_buffer.resize(total_size, Cell{});
        _front_buffer.resize(total_size, Cell{});
    }

    std::size_t        _width  = 0;
    std::size_t        _height = 0;
    std::vector<Cell> _back_buffer;
    std::vector<Cell> _front_buffer;
};

} // namespace dungeon::display
