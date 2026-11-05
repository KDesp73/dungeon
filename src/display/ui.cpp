#include "dungeon/display/ui.hpp"

#include "dungeon/display/style.hpp"
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
constexpr CellStyle kSelectionStyle{Color::Black, Color::BrightGreen, TextStyle::Bold};

constexpr bool has_border(const Rect& rect) noexcept {
    return rect.width >= 2 && rect.height >= 2;
}

constexpr CellStyle entity_style(const dungeon::map::entities::Entity& entity) noexcept {
    return CellStyle{entity.color(), Color::Default, entity.style()};
}

constexpr CellStyle tile_style(dungeon::map::Tile tile) noexcept {
    using dungeon::map::Tile;

    switch (tile) {
    case Tile::Empty:
        return CellStyle{};
    case Tile::HWall:
    case Tile::VWall:
        return CellStyle{Color::Default, Color::Default, TextStyle::Bold};
    case Tile::Walkway:
        return CellStyle{Color::BrightBlack, Color::Default, TextStyle::None};
    case Tile::Floor:
        return CellStyle{Color::BrightBlack, Color::Default, TextStyle::None};
    case Tile::Door:
        return CellStyle{Color::Yellow, Color::Default, TextStyle::Bold};
    }

    return CellStyle{};
}

/// Draws at most @p max_width code points of @p text.
void draw_clipped(FrameBuffer& buffer, std::string_view text, std::size_t x, std::size_t y,
                  std::size_t max_width, CellStyle style = {}) {
    std::size_t offset    = 0;
    std::size_t column    = 0;
    char32_t    codepoint = U'\0';

    while (offset < text.size() && column < max_width) {
        const std::size_t consumed = utf8::decode(text, offset, codepoint);
        if (consumed == 0) {
            break;
        }

        buffer.draw(x + column, y, codepoint, style);
        offset += consumed;
        ++column;
    }
}

/// Returns the chest the player is standing on or orthogonally next to.
const dungeon::map::entities::Chest* nearby_chest(const dungeon::map::Map& map) {
    const auto* player = map.player();
    if (player == nullptr) {
        return nullptr;
    }

    for (const dungeon::map::EntityPtr& entity : map.entity_ptrs()) {
        const auto* chest = dynamic_cast<const dungeon::map::entities::Chest*>(entity.get());
        if (chest == nullptr) {
            continue;
        }

        const std::size_t dx = chest->x > player->x ? chest->x - player->x : player->x - chest->x;
        const std::size_t dy = chest->y > player->y ? chest->y - player->y : player->y - chest->y;

        if (dx + dy <= 1) {
            return chest;
        }
    }

    return nullptr;
}

/// Draws item slots in the style shared by the inventory and chest panels.
void draw_item_slots(FrameBuffer& buffer, const Rect& rect,
                     const std::vector<dungeon::map::entities::Item>* items, std::size_t first,
                     std::size_t selected, bool has_selection) {
    const std::size_t slot_count = std::min<std::size_t>((rect.width - 4) / 4, 10);
    const std::size_t y          = rect.y + 2;
    std::size_t       column     = rect.x + 2;

    for (std::size_t i = 0; i < slot_count; ++i) {
        if (i > 0) {
            ++column;
        }

        const std::size_t index       = first + i;
        const bool        is_selected = has_selection && index == selected;
        const CellStyle   slot_style  = is_selected ? kSelectionStyle : CellStyle{};

        buffer.draw(column++, y, '[', slot_style);

        if (items != nullptr && index < items->size()) {
            const dungeon::map::entities::Item& item = (*items)[index];
            buffer.draw(column++, y, item.tile(),
                        is_selected ? kSelectionStyle : entity_style(item));
        } else {
            buffer.draw(column++, y, ' ', slot_style);
        }

        buffer.draw(column++, y, ']', slot_style);
    }
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

    const std::vector<dungeon::map::entities::Item>* items =
        player != nullptr ? &player->inventory() : nullptr;

    draw_item_slots(buffer, rect, items, 0, 0, false);
}

void draw_messages(FrameBuffer& buffer, const Rect& rect, std::span<const std::string> messages) {
    panel(buffer, rect, "MESSAGES");

    if (!has_border(rect) || rect.width <= 4) {
        return;
    }

    const std::size_t inner_width = rect.width - 4;
    const std::size_t bottom      = rect.y + rect.height - 1;
    std::size_t       row         = rect.y + rect.height - 2;

    if (messages.empty()) {
        return;
    }

    std::ptrdiff_t index = static_cast<std::ptrdiff_t>(messages.size()) - 1;
    while (index >= 0 && row > rect.y + 1 && row < bottom) {
        draw_clipped(buffer, messages[static_cast<std::size_t>(index)], rect.x + 2, row,
                     inner_width, CellStyle{});
        --row;
        --index;
    }
}

void draw_chest(FrameBuffer& buffer, const Rect& rect, const dungeon::map::entities::Chest* chest,
                std::size_t selected) {
    panel(buffer, rect, "CHEST");

    if (!has_border(rect) || rect.width <= 4) {
        return;
    }

    if (chest == nullptr) {
        draw_clipped(buffer, "No chest nearby.", rect.x + 2, rect.y + 2, rect.width - 4,
                     kBorderStyle);
        return;
    }

    if (chest->empty()) {
        draw_clipped(buffer, "Empty.", rect.x + 2, rect.y + 2, rect.width - 4, kBorderStyle);
        return;
    }

    const std::size_t selected_index = std::min(selected, chest->contents().size() - 1);
    const std::size_t slot_count     = std::min<std::size_t>((rect.width - 4) / 4, 10);
    const std::size_t first = selected_index >= slot_count ? selected_index - slot_count + 1 : 0;

    draw_item_slots(buffer, rect, &chest->contents(), first, selected_index, true);
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

    const std::size_t bottom_width    = width - 2 * kUiGap;
    const std::size_t inventory_width = bottom_width / 3;
    const std::size_t messages_width  = bottom_width / 3;
    const std::size_t chest_width     = bottom_width - inventory_width - messages_width;

    layout.game      = Rect{0, 0, game_width, game_height};
    layout.stats     = Rect{game_width + kUiGap, 0, stats_width, game_height};
    layout.inventory = Rect{0, game_height + kUiGap, inventory_width, inventory_height};
    layout.messages =
        Rect{inventory_width + kUiGap, game_height + kUiGap, messages_width, inventory_height};
    layout.chest = Rect{inventory_width + kUiGap + messages_width + kUiGap, game_height + kUiGap,
                        chest_width, inventory_height};

    return layout;
}

void label(FrameBuffer& buffer, std::string_view text, std::size_t x, std::size_t y,
           CellStyle style) {
    std::size_t offset    = 0;
    std::size_t column    = 0;
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

std::size_t text_width(std::string_view text) noexcept {
    std::size_t width     = 0;
    std::size_t offset    = 0;
    char32_t    codepoint = U'\0';

    while (offset < text.size()) {
        const std::size_t consumed = utf8::decode(text, offset, codepoint);
        if (consumed == 0) {
            break;
        }

        offset += consumed;
        ++width;
    }

    return width;
}

void fill(FrameBuffer& buffer, const Rect& rect, char32_t glyph, CellStyle style) {
    for (std::size_t y = rect.y; y < rect.y + rect.height; ++y) {
        for (std::size_t x = rect.x; x < rect.x + rect.width; ++x) {
            buffer.draw(x, y, glyph, style);
        }
    }
}

void label_centered(FrameBuffer& buffer, std::string_view text, const Rect& rect, std::size_t y,
                    CellStyle style) {
    const std::size_t width   = text_width(text);
    const std::size_t visible = std::min(width, rect.width);
    const std::size_t x       = rect.x + (rect.width > visible ? (rect.width - visible) / 2 : 0);

    draw_clipped(buffer, text, x, y, visible, style);
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

    // Draw dungeon
    for (std::size_t y = 0; y < map_height; ++y) {
        for (std::size_t x = 0; x < map_width; ++x) {
            const dungeon::map::Tile tile = map.at(x, y);
            buffer.draw(offset_x + x, offset_y + y, dungeon::map::glyph(tile), tile_style(tile));
        }
    }

    // Draw exit if unlocked
    if (map.exit_unlocked()) {
        const dungeon::map::Point exit = map.exit();
        if (exit.x < map_width && exit.y < map_height) {
            buffer.draw(offset_x + exit.x, offset_y + exit.y, dungeon::map::EXIT_GLYPH, kExitStyle);
        }
    }

    // Draw entities
    for (const auto& entity : map.entity_ptrs()) {
        if (entity) {
            buffer.draw(offset_x + entity->x, offset_y + entity->y, entity->tile(),
                        entity_style(*entity));
        }
    }
}

void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map, const Layout& layout,
             const HudState& hud) {
    panel(buffer, layout.game, "DUNGEON");
    draw_map(buffer, map, layout.game);

    draw_stats(buffer, layout.stats, map.seed(), map.player());
    draw_inventory(buffer, layout.inventory, map.player());
    draw_messages(buffer, layout.messages, hud.messages);
    draw_chest(buffer, layout.chest, nearby_chest(map), hud.selected_chest_item);
}

void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map, const Layout& layout) {
    draw_ui(buffer, map, layout, HudState{});
}

void draw_ui(FrameBuffer& buffer, const dungeon::map::Map& map, const HudState& hud) {
    draw_ui(buffer, map, make_layout(buffer.width(), buffer.height()), hud);
}

} // namespace dungeon::display::tui
