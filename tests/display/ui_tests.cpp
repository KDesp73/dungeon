#include "test_harness.hpp"

#include "dungeon/display/buffer.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/game.hpp"
#include "dungeon/map/map.hpp"

#include <algorithm>
#include <string>
#include <utility>

using dungeon::display::FrameBuffer;
using dungeon::display::tui::HudState;
using dungeon::display::tui::Layout;
using dungeon::display::tui::make_layout;
using dungeon::game::Direction;
using dungeon::game::Game;
using dungeon::map::DoorSide;
using dungeon::map::MapSpec;
using dungeon::map::entities::EnemyKind;
using dungeon::map::entities::ItemKind;

namespace {

MapSpec make_key_spec() {
    MapSpec spec;
    spec.width       = 20;
    spec.height      = 10;
    spec.rooms       = {{2, 2, 6, 5}, {12, 2, 6, 5}};
    spec.connections = {{0, 1, DoorSide::Right, DoorSide::Left}};
    spec.entrance    = {3, 3};
    spec.exit        = {15, 3};
    spec.items       = {{ItemKind::Key, 4, 3}};
    return spec;
}

std::pair<std::size_t, std::size_t> map_origin(const Game& game, const Layout& layout) {
    const std::size_t inner_width  = layout.game.width - 2;
    const std::size_t inner_height = layout.game.height - 2;
    const std::size_t map_width    = std::min(game.map().width(), inner_width);
    const std::size_t map_height   = std::min(game.map().height(), inner_height);

    return {layout.game.x + 1 + (inner_width - map_width) / 2,
            layout.game.y + 1 + (inner_height - map_height) / 2};
}

std::string row_text(const FrameBuffer& buffer, std::size_t x, std::size_t y,
                     std::size_t width) {
    std::string text;
    text.reserve(width);

    for (std::size_t i = 0; i < width; ++i) {
        text.push_back(buffer.at(x + i, y));
    }

    return text;
}

} // namespace

DUNGEON_TEST(display, ui_layout_places_game_stats_and_bottom_panels) {
    const Layout layout = make_layout(100, 30);

    CHECK_EQ(layout.game.x, 0U);
    CHECK_EQ(layout.game.y, 0U);
    CHECK_EQ(layout.stats.x, layout.game.width + 1);
    CHECK_EQ(layout.stats.y, 0U);
    CHECK_EQ(layout.inventory.x, 0U);
    CHECK_EQ(layout.inventory.y, layout.game.height + 1);
    CHECK_EQ(layout.messages.x, layout.inventory.width + 1);
    CHECK_EQ(layout.messages.y, layout.inventory.y);
    CHECK_EQ(layout.chest.x, layout.messages.x + layout.messages.width + 1);
    CHECK_EQ(layout.chest.y, layout.inventory.y);

    CHECK_EQ(layout.game.width + 1 + layout.stats.width, 100U);
    CHECK_EQ(layout.game.height + 1 + layout.inventory.height, 30U);
    CHECK_EQ(layout.inventory.width + layout.messages.width + layout.chest.width + 2, 100U);
    CHECK_EQ(layout.inventory.height, layout.messages.height);
    CHECK_EQ(layout.inventory.height, layout.chest.height);

    CHECK(layout.game.width > layout.stats.width);
    CHECK_EQ(layout.stats.height, layout.game.height);
}

DUNGEON_TEST(display, ui_layout_is_empty_on_a_small_framebuffer) {
    const Layout layout = make_layout(30, 10);

    CHECK_EQ(layout.game.width, 0U);
    CHECK_EQ(layout.game.height, 0U);
    CHECK_EQ(layout.stats.width, 0U);
    CHECK_EQ(layout.stats.height, 0U);
    CHECK_EQ(layout.inventory.width, 0U);
    CHECK_EQ(layout.inventory.height, 0U);
    CHECK_EQ(layout.messages.width, 0U);
    CHECK_EQ(layout.messages.height, 0U);
    CHECK_EQ(layout.chest.width, 0U);
    CHECK_EQ(layout.chest.height, 0U);
}

DUNGEON_TEST(display, ui_draws_a_generated_map) {
    FrameBuffer buffer(100, 30);

    dungeon::map::Map map;
    map.generate(74, 24);

    dungeon::display::tui::draw_ui(buffer, map, make_layout(buffer.width(), buffer.height()));
}

DUNGEON_TEST(display, ui_shows_equipped_items_in_the_inventory) {
    Game game(make_key_spec());
    REQUIRE(game.move(Direction::Right));

    FrameBuffer  buffer(100, 30);
    const Layout layout = make_layout(buffer.width(), buffer.height());
    dungeon::display::tui::draw_ui(buffer, game.map(), layout);

    const std::size_t x = layout.inventory.x + 2;
    const std::size_t y = layout.inventory.y + 2;

    CHECK_EQ(buffer.at(x, y), '[');
    CHECK_EQ(buffer.at(x + 1, y), 'k');
    CHECK_EQ(buffer.at(x + 2, y), ']');
}

DUNGEON_TEST(display, ui_shows_a_health_potion_in_the_inventory) {
    MapSpec spec = make_key_spec();
    spec.items   = {{ItemKind::HealthPotion, 4, 3}};

    Game game(spec);
    REQUIRE(game.move(Direction::Right));

    FrameBuffer  buffer(100, 30);
    const Layout layout = make_layout(buffer.width(), buffer.height());
    dungeon::display::tui::draw_ui(buffer, game.map(), layout);

    const std::size_t x = layout.inventory.x + 2;
    const std::size_t y = layout.inventory.y + 2;

    CHECK_EQ(buffer.at(x, y), '[');
    CHECK_EQ(buffer.at(x + 1, y), '!');
    CHECK_EQ(buffer.at(x + 2, y), ']');
}

DUNGEON_TEST(display, ui_shows_the_contents_of_a_nearby_chest) {
    MapSpec spec = make_key_spec();
    spec.items   = {{ItemKind::Chest,
                     5,
                     3,
                     {ItemKind::Coin, ItemKind::Key}}};

    Game game(spec);
    REQUIRE(game.move(Direction::Right));

    FrameBuffer  buffer(100, 30);
    const Layout layout = make_layout(buffer.width(), buffer.height());
    const HudState hud{game.messages(), game.chest_selection()};
    dungeon::display::tui::draw_ui(buffer, game.map(), layout, hud);

    const std::size_t x = layout.chest.x + 2;
    const std::size_t y = layout.chest.y + 2;

    CHECK_EQ(buffer.at(x, y), '[');
    CHECK_EQ(buffer.at(x + 1, y), '$');
    CHECK_EQ(buffer.at(x + 2, y), ']');
    CHECK_EQ(buffer.at(x + 4, y), '[');
    CHECK_EQ(buffer.at(x + 5, y), 'k');
    CHECK_EQ(buffer.at(x + 6, y), ']');
}

DUNGEON_TEST(display, ui_highlights_the_selected_chest_item) {
    MapSpec spec = make_key_spec();
    spec.items   = {{ItemKind::Chest,
                     5,
                     3,
                     {ItemKind::Coin, ItemKind::Key}}};

    Game game(spec);
    REQUIRE(game.move(Direction::Right));
    game.cycle_chest_selection(1);

    FrameBuffer  buffer(100, 30);
    const Layout layout = make_layout(buffer.width(), buffer.height());
    const HudState hud{game.messages(), game.chest_selection()};
    dungeon::display::tui::draw_ui(buffer, game.map(), layout, hud);

    const std::size_t x = layout.chest.x + 2;
    const std::size_t y = layout.chest.y + 2;

    CHECK(buffer.cell_at(x + 1, y).style != dungeon::display::CellStyle{});
    CHECK_EQ(buffer.cell_at(x + 5, y).style.background, dungeon::display::Color::BrightGreen);
    CHECK_EQ(buffer.cell_at(x + 1, y).style.background, dungeon::display::Color::Default);
}

DUNGEON_TEST(display, ui_shows_game_messages_in_the_middle_panel) {
    MapSpec spec = make_key_spec();
    spec.items   = {{ItemKind::Coin, 4, 3}};

    Game game(spec);
    REQUIRE(game.move(Direction::Right));

    FrameBuffer  buffer(100, 30);
    const Layout layout = make_layout(buffer.width(), buffer.height());
    const HudState hud{game.messages(), game.chest_selection()};
    dungeon::display::tui::draw_ui(buffer, game.map(), layout, hud);

    const std::string expected = "+1 gold";
    const std::size_t x        = layout.messages.x + 2;
    const std::size_t y        = layout.messages.y + layout.messages.height - 2;

    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK_EQ(buffer.at(x + i, y), expected[i]);
    }
}

DUNGEON_TEST(display, ui_shows_gold_after_collecting_a_coin) {
    MapSpec spec = make_key_spec();
    spec.items   = {{ItemKind::Coin, 4, 3}};

    Game game(spec);
    REQUIRE(game.move(Direction::Right));

    FrameBuffer  buffer(100, 30);
    const Layout layout = make_layout(buffer.width(), buffer.height());
    dungeon::display::tui::draw_ui(buffer, game.map(), layout);

    const std::string gold = "Gold:     1";
    const std::size_t x    = layout.stats.x + 2;
    const std::size_t y    = layout.stats.y + 8;

    for (std::size_t i = 0; i < gold.size(); ++i) {
        CHECK_EQ(buffer.at(x + i, y), gold[i]);
    }
}

DUNGEON_TEST(display, ui_shows_the_nearest_enemy_stats_in_the_right_panel) {
    MapSpec spec = make_key_spec();
    spec.enemies = {{EnemyKind::Fast, 5, 3}};

    Game game(spec);

    FrameBuffer  buffer(100, 30);
    const Layout layout = make_layout(buffer.width(), buffer.height());
    dungeon::display::tui::draw_ui(buffer, game.map(), layout);

    const std::size_t x = layout.stats.x + 2;
    const std::size_t width = layout.stats.width - 4;

    bool found_name    = false;
    bool found_attack  = false;
    bool found_defence = false;

    for (std::size_t y = layout.stats.y + 1; y < layout.stats.y + layout.stats.height - 1; ++y) {
        const std::string text = row_text(buffer, x, y, width);

        if (text.find("Rat (Fast)") != std::string::npos) {
            found_name = true;
        }
        if (text.find("Attack:   2") != std::string::npos) {
            found_attack = true;
        }
        if (text.find("Defense:  1") != std::string::npos) {
            found_defence = true;
        }
    }

    CHECK(found_name);
    CHECK(found_attack);
    CHECK(found_defence);
}

DUNGEON_TEST(display, ui_marks_the_exit_after_the_key_is_equipped) {
    Game game(make_key_spec());
    REQUIRE(game.move(Direction::Right));

    FrameBuffer  buffer(100, 30);
    const Layout layout = make_layout(buffer.width(), buffer.height());
    dungeon::display::tui::draw_ui(buffer, game.map(), layout);

    const auto [origin_x, origin_y] = map_origin(game, layout);
    const dungeon::map::Point exit  = game.map().exit();

    CHECK_EQ(buffer.glyph_at(origin_x + exit.x, origin_y + exit.y),
             dungeon::map::EXIT_GLYPH);
}
