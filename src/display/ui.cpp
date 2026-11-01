#include "dungeon/display/ui.hpp"

#include "dungeon/display/utf8.hpp"
#include "dungeon/map/map.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace dungeon::display::tui {

namespace {

constexpr std::size_t kUiGap = 1;

constexpr CellStyle kBorderStyle{Color::BrightBlack, Color::Default, TextStyle::None};
constexpr CellStyle kTitleStyle{Color::BrightWhite, Color::Default, TextStyle::Bold};
constexpr CellStyle kExitStyle{Color::BrightGreen, Color::Default, TextStyle::Bold};

constexpr bool has_border(const Rect& rect) noexcept {
    return rect.width >= 2 && rect.height >= 2;
}

constexpr Color to_color(dungeon::map::entities::Color color) noexcept {
    using dungeon::map::entities::Color;

    switch (color) {
    case Color::Red:
        return dungeon::display::Color::Red;
    case Color::Blue:
        return dungeon::display::Color::Blue;
    case Color::Green:
        return dungeon::display::Color::Green;
    case Color::Yellow:
        return dungeon::display::Color::Yellow;
    case Color::Grey:
        return dungeon::display::Color::BrightBlack;
    case Color::Black:
        return dungeon::display::Color::Black;
    case Color::White:
        return dungeon::display::Color::White;
    case Color::Cyan:
        return dungeon::display::Color::Cyan;
    case Color::Purple:
        return dungeon::display::Color::Magenta;
    }

    return dungeon::display::Color::Default;
}

constexpr TextStyle to_text_style(dungeon::map::entities::Style style) noexcept {
    using dungeon::map::entities::Style;

    switch (style) {
    case Style::None:
        return TextStyle::None;
    case Style::Underline:
        return TextStyle::Underline;
    case Style::Bold:
        return TextStyle::Bold;
    case Style::Italic:
        return TextStyle::Italic;
    }

    return TextStyle::None;
}

constexpr CellStyle entity_style(const dungeon::map::entities::Entity& entity) noexcept {
    return CellStyle{to_color(entity.color()), Color::Default, to_text_style(entity.style())};
}

constexpr CellStyle tile_style(dungeon::map::Tile tile) noexcept {
    using dungeon::map::Tile;

    switch (tile) {
    case Tile::Empty:
        return CellStyle{};
    case Tile::HWall:
    case Tile::VWall:
    case Tile::Walkway:
        return CellStyle{Color::BrightBlack, Color::Default, TextStyle::None};
    case Tile::Floor:
        return CellStyle{Color::Default, Color::Default, TextStyle::None};
    case Tile::Door:
        return CellStyle{Color::Yellow, Color::Default, TextStyle::Bold};
    }

    return CellStyle{};
}

void draw_stats(FrameBuffer& buffer, const Rect& rect, std::uint32_t seed,
                const dungeon::map::entities::Player* player) {
    panel(buffer, rect, "STATS");

    if (!has_border(rect) || rect.width <= 4) {
        return;
    }

    const dungeon::map::entities::Stats stats =
        player != nullptr ? player->stats() : dungeon::map::entities::Stats{};

    const std::size_t left   = rect.x + 2;
    const std::size_t bottom = rect.y + rect.height - 1;
    std::size_t       row    = rect.y + 2;

    const auto line = [&](std::string_view text) {
        if (row >= bottom) {
            return;
        }
        label(buffer, text, left, row);
        ++row;
    };

    line("Level:    " + std::to_string(stats.level));
    line("HP:       " + std::to_string(stats.hp));
    line("Stamina:  " + std::to_string(stats.stamina));
    line("Attack:   " + std::to_string(stats.attack));
    line("Defense:  " + std::to_string(stats.defence));
    line("Gold:     " + std::to_string(stats.gold));
    ++row;
    line("Seed:");
    line(std::to_string(seed));
}

void draw_inventory(FrameBuffer& buffer, const Rect& rect,
                    const dungeon::map::entities::Player* player) {
    panel(buffer, rect, "INVENTORY");

    if (!has_border(rect) || rect.width <= 4) {
        return;
    }

    const std::size_t slot_count = std::min<std::size_t>((rect.width - 4) / 4, 10);
    const auto*       inventory  = player != nullptr ? &player->inventory() : nullptr;

    const std::size_t y      = rect.y + 2;
    std::size_t       column = rect.x + 2;

    for (std::size_t i = 0; i < slot_count; ++i) {
        if (i > 0) {
            ++column;
        }

        buffer.draw(column++, y, '[');

        if (inventory != nullptr && i < inventory->size()) {
            const dungeon::map::entities::Item& item = (*inventory)[i];
            buffer.draw(column++, y, item.tile(), entity_style(item));
        } else {
            buffer.draw(column++, y, ' ');
        }

        buffer.draw(column++, y, ']');
    }
}

} // namespace

Layout make_layout(std::size_t width, std::size_t height) {
    Layout layout;

    constexpr std::size_t min_width  = 40;
    constexpr std::size_t min_height = 12;

    if (width < min_width || height < min_height) {
        return layout;
    }

    const std::size_t stats_width      = std::clamp(width / 4, std::size_t{20}, std::size_t{28});
    const std::size_t inventory_height = std::clamp(height / 8, std::size_t{5}, std::size_t{7});

    const std::size_t game_width  = width - stats_width - kUiGap;
    const std::size_t game_height = height - inventory_height - kUiGap;

    layout.game      = Rect{0, 0, game_width, game_height};
    layout.stats     = Rect{game_width + kUiGap, 0, stats_width, game_height};
    layout.inventory = Rect{0, game_height + kUiGap, width, inventory_height};

    return layout;
}

void label(FrameBuffer& buffer, std::string_view text, std::size_t x, std::size_t y,
           CellStyle style) {
    std::size_t offset = 0;
    std::size_t column = 0;
    char32_t    codepoint = U'\0';

    while (offset < text.size()) {
        const std::size_t consumed = utf8::decode(text, offset, codepoint);
        if (consumed == 0) {
            break;
        }

        buffer.draw(x + column, y, codepoint, style);
        offset += consumed;
        ++column;
    }
}

void box(FrameBuffer& buffer, const Rect& rect) {
    if (!has_border(rect)) {
        return;
    }

    const std::size_t right  = rect.x + rect.width - 1;
    const std::size_t bottom = rect.y + rect.height - 1;

    for (std::size_t x = rect.x + 1; x < right; ++x) {
        buffer.draw(x, rect.y, U'─', kBorderStyle);
        buffer.draw(x, bottom, U'─', kBorderStyle);
    }

    for (std::size_t y = rect.y + 1; y < bottom; ++y) {
        buffer.draw(rect.x, y, U'│', kBorderStyle);
        buffer.draw(right, y, U'│', kBorderStyle);
    }

    buffer.draw(rect.x, rect.y, U'┌', kBorderStyle);
    buffer.draw(right, rect.y, U'┐', kBorderStyle);
    buffer.draw(rect.x, bottom, U'└', kBorderStyle);
    buffer.draw(right, bottom, U'┘', kBorderStyle);
}

void panel(FrameBuffer& buffer, const Rect& rect, std::string_view title) {
    box(buffer, rect);

    if (!has_border(rect) || title.empty() || rect.width <= 4) {
        return;
    }

    const std::string_view shown = title.substr(0, rect.width - 4);
    label(buffer, shown, rect.x + 2, rect.y, kTitleStyle);

    const std::size_t title_end = rect.x + 2 + shown.size();
    if (title_end < rect.x + rect.width - 1) {
        buffer.draw(title_end, rect.y, ' ');
    }
}

void draw_map(FrameBuffer& buffer, const dungeon::map::Map& map, const Rect& viewport) {
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
            const dungeon::map::Tile tile = map.at(x, y);
            buffer.draw(offset_x + x, offset_y + y, dungeon::map::glyph(tile), tile_style(tile));
        }
    }

    if (map.exit_unlocked()) {
        const dungeon::map::Point exit = map.exit();
        if (exit.x < map_width && exit.y < map_height) {
            buffer.draw(offset_x + exit.x, offset_y + exit.y, 'E', kExitStyle);
        }
    }

    for (const auto& entity : map.entity_ptrs()) {
        if (entity) {
            buffer.draw(offset_x + entity->x, offset_y + entity->y, entity->tile(),
                        entity_style(*entity));
        }
    }
}

void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map, const Layout& layout) {
    panel(buffer, layout.game, "DUNGEON");
    draw_map(buffer, map, layout.game);

    draw_stats(buffer, layout.stats, map.seed(), map.player());
    draw_inventory(buffer, layout.inventory, map.player());
}

void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map) {
    draw_ui(buffer, map, make_layout(buffer.width(), buffer.height()));
}

} // namespace dungeon::display::tui
