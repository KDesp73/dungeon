#include "test_harness.hpp"

#include "dungeon/display/buffer.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/game.hpp"
#include "dungeon/map/map.hpp"

#include <algorithm>
#include <string>
#include <utility>

using dungeon::display::FrameBuffer;
using dungeon::display::tui::Layout;
using dungeon::display::tui::make_layout;
using dungeon::game::Direction;
using dungeon::game::Game;
using dungeon::map::DoorSide;
using dungeon::map::MapSpec;
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

} // namespace

DUNGEON_TEST(display, ui_layout_places_game_stats_and_inventory) {
    const Layout layout = make_layout(100, 30);

    CHECK_EQ(layout.game.x, 0U);
    CHECK_EQ(layout.game.y, 0U);
    CHECK_EQ(layout.stats.x, layout.game.width + 1);
    CHECK_EQ(layout.stats.y, 0U);
    CHECK_EQ(layout.inventory.x, 0U);
    CHECK_EQ(layout.inventory.y, layout.game.height + 1);

    CHECK_EQ(layout.game.width + 1 + layout.stats.width, 100U);
    CHECK_EQ(layout.game.height + 1 + layout.inventory.height, 30U);
    CHECK_EQ(layout.inventory.width, 100U);

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
    const std::size_t y    = layout.stats.y + 7;

    for (std::size_t i = 0; i < gold.size(); ++i) {
        CHECK_EQ(buffer.at(x + i, y), gold[i]);
    }
}

DUNGEON_TEST(display, ui_marks_the_exit_after_the_key_is_equipped) {
    Game game(make_key_spec());
    REQUIRE(game.move(Direction::Right));

    FrameBuffer  buffer(100, 30);
    const Layout layout = make_layout(buffer.width(), buffer.height());
    dungeon::display::tui::draw_ui(buffer, game.map(), layout);

    const auto [origin_x, origin_y] = map_origin(game, layout);
    const dungeon::map::Point exit  = game.map().exit();

    CHECK_EQ(buffer.at(origin_x + exit.x, origin_y + exit.y), 'E');
}
