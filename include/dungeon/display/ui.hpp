#pragma once

#include "dungeon/display/buffer.hpp"

#include <cstddef>
#include <string_view>

namespace dungeon::map {
class Map;
}

namespace dungeon::display::tui {

/// A rectangle in framebuffer coordinates.
struct Rect {
    std::size_t x      = 0;
    std::size_t y      = 0;
    std::size_t width  = 0;
    std::size_t height = 0;
};

/// Screen regions used by the main game UI.
///
/// The game viewport occupies the large area on the left, the stats column sits
/// on the right, and the inventory spans the bottom of the screen.
struct Layout {
    Rect game;
    Rect stats;
    Rect inventory;
};

/// Splits a framebuffer of @p width by @p height into the game UI regions.
///
/// Returns an empty layout when the framebuffer is too small to lay out
/// meaningfully.
[[nodiscard]] Layout make_layout(std::size_t width, std::size_t height);

/// Writes @p text starting at (@p x, @p y).
///
/// The text is decoded as UTF-8 and each code point is drawn with @p style.
void label(FrameBuffer& buffer, std::string_view text, std::size_t x, std::size_t y,
           CellStyle style = {});

/// Draws a single-line border around @p rect.
void box(FrameBuffer& buffer, const Rect& rect);

/// Draws a bordered panel with @p title written into its top border.
void panel(FrameBuffer& buffer, const Rect& rect, std::string_view title);

/// Draws @p map centred inside @p viewport, clipped to its interior.
void draw_map(FrameBuffer& buffer, const dungeon::map::Map& map, const Rect& viewport);

/// Draws the game viewport, stats column, and inventory.
void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map, const Layout& layout);

/// Computes a layout for @p buffer and draws the UI into it.
void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map);

} // namespace dungeon::display::tui
