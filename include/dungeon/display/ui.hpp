#pragma once

#include "dungeon/display/buffer.hpp"

#include <cstddef>
#include <span>
#include <string>
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
/// on the right, and the bottom row is split into inventory, messages, and
/// chest panels.
struct Layout {
    Rect game;
    Rect stats;
    Rect inventory;
    Rect messages;
    Rect chest;
};

/// Game state the bottom panels need in addition to the map.
struct HudState {
    /// Messages to show, oldest first. The newest messages are drawn last.
    std::span<const std::string> messages{};

    /// Index of the highlighted chest item.
    std::size_t selected_chest_item = 0;
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

/// Returns the number of code points in @p text.
[[nodiscard]] std::size_t text_width(std::string_view text) noexcept;

/// Fills @p rect with @p glyph drawn in @p style.
void fill(FrameBuffer& buffer, const Rect& rect, char32_t glyph = U' ', CellStyle style = {});

/// Writes @p text centred horizontally inside @p rect on row @p y.
void label_centered(FrameBuffer& buffer, std::string_view text, const Rect& rect, std::size_t y,
                    CellStyle style = {});

/// Draws a single-line border around @p rect.
void box(FrameBuffer& buffer, const Rect& rect);

/// Draws a bordered panel with @p title written into its top border.
void panel(FrameBuffer& buffer, const Rect& rect, std::string_view title);

/// Draws @p map centred inside @p viewport, clipped to its interior.
void draw_map(FrameBuffer& buffer, const dungeon::map::Map& map, const Rect& viewport);

/// Draws the game viewport, stats column, and bottom panels.
void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map, const Layout& layout,
             const HudState& hud);

/// Draws the UI with an empty HUD state.
void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map, const Layout& layout);

/// Computes a layout for @p buffer and draws the UI into it.
void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map, const HudState& hud = {});

} // namespace dungeon::display::tui
