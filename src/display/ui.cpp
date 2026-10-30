#include "dungeon/display/ui.hpp"

#include "dungeon/map/map.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace dungeon::display::tui {

namespace {

constexpr std::size_t kUiGap = 1;

constexpr bool has_border(const Rect& rect) noexcept
{
    return rect.width >= 2 && rect.height >= 2;
}

void draw_stats(FrameBuffer& buffer, const Rect& rect, std::uint32_t seed)
{
    panel(buffer, rect, "STATS");

    if (!has_border(rect) || rect.width <= 4) {
        return;
    }

    const std::size_t left   = rect.x + 2;
    const std::size_t bottom = rect.y + rect.height - 1;
    std::size_t row          = rect.y + 2;

    const auto line = [&](std::string_view text) {
        if (row >= bottom) {
            return;
        }
        label(buffer, text, left, row);
        ++row;
    };

    line("Level:    1");
    line("HP:       100/100");
    line("Stamina:  100/100");
    line("Attack:   10");
    line("Defense:  5");
    line("Gold:     0");
    ++row;
    line("Seed:");
    line(std::to_string(seed));
}

void draw_inventory(FrameBuffer& buffer, const Rect& rect)
{
    panel(buffer, rect, "INVENTORY");

    if (!has_border(rect) || rect.width <= 4) {
        return;
    }

    const std::size_t slot_count = std::min<std::size_t>((rect.width - 4) / 4, 10);
    std::string       slots;

    for (std::size_t i = 0; i < slot_count; ++i) {
        if (!slots.empty()) {
            slots += ' ';
        }
        slots += "[ ]";
    }

    if (!slots.empty()) {
        label(buffer, slots, rect.x + 2, rect.y + 2);
    }
}

} // namespace

Layout make_layout(std::size_t width, std::size_t height)
{
    Layout layout;

    constexpr std::size_t min_width  = 40;
    constexpr std::size_t min_height = 12;

    if (width < min_width || height < min_height) {
        return layout;
    }

    const std::size_t stats_width = std::clamp(width / 4, std::size_t{20}, std::size_t{28});
    const std::size_t inventory_height =
        std::clamp(height / 8, std::size_t{5}, std::size_t{7});

    const std::size_t game_width  = width - stats_width - kUiGap;
    const std::size_t game_height = height - inventory_height - kUiGap;

    layout.game      = Rect{0, 0, game_width, game_height};
    layout.stats     = Rect{game_width + kUiGap, 0, stats_width, game_height};
    layout.inventory = Rect{0, game_height + kUiGap, width, inventory_height};

    return layout;
}

void label(FrameBuffer& buffer, std::string_view text, std::size_t x, std::size_t y)
{
    for (std::size_t i = 0; i < text.size(); ++i) {
        buffer.draw(x + i, y, text[i]);
    }
}

void box(FrameBuffer& buffer, const Rect& rect)
{
    if (!has_border(rect)) {
        return;
    }

    const std::size_t right  = rect.x + rect.width - 1;
    const std::size_t bottom = rect.y + rect.height - 1;

    for (std::size_t x = rect.x; x <= right; ++x) {
        buffer.draw(x, rect.y, '-');
        buffer.draw(x, bottom, '-');
    }

    for (std::size_t y = rect.y; y <= bottom; ++y) {
        buffer.draw(rect.x, y, '|');
        buffer.draw(right, y, '|');
    }

    buffer.draw(rect.x, rect.y, '+');
    buffer.draw(right, rect.y, '+');
    buffer.draw(rect.x, bottom, '+');
    buffer.draw(right, bottom, '+');
}

void panel(FrameBuffer& buffer, const Rect& rect, std::string_view title)
{
    box(buffer, rect);

    if (!has_border(rect) || title.empty() || rect.width <= 4) {
        return;
    }

    const std::string_view shown = title.substr(0, rect.width - 4);
    label(buffer, shown, rect.x + 2, rect.y);

    const std::size_t title_end = rect.x + 2 + shown.size();
    if (title_end < rect.x + rect.width - 1) {
        buffer.draw(title_end, rect.y, ' ');
    }
}

void draw_map(FrameBuffer& buffer, const dungeon::map::Map& map, const Rect& viewport)
{
    if (!has_border(viewport) || map.empty()) {
        return;
    }

    const std::size_t inner_width  = viewport.width - 2;
    const std::size_t inner_height = viewport.height - 2;
    const std::size_t map_width    = std::min(map.width(), inner_width);
    const std::size_t map_height   = std::min(map.height(), inner_height);

    const std::size_t offset_x = viewport.x + 1 + (inner_width - map_width) / 2;
    const std::size_t offset_y = viewport.y + 1 + (inner_height - map_height) / 2;

    for (std::size_t y = 0; y < map_height; ++y) {
        for (std::size_t x = 0; x < map_width; ++x) {
            buffer.draw(offset_x + x, offset_y + y, dungeon::map::glyph(map.at(x, y)));
        }
    }
}

void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map, const Layout& layout)
{
    panel(buffer, layout.game, "DUNGEON");
    draw_map(buffer, map, layout.game);

    draw_stats(buffer, layout.stats, map.seed());
    draw_inventory(buffer, layout.inventory);
}

void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map)
{
    draw_ui(buffer, map, make_layout(buffer.width(), buffer.height()));
}

} // namespace dungeon::display::tui
