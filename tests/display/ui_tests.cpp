#include "test_harness.hpp"

#include "dungeon/display/buffer.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/map/map.hpp"

using dungeon::display::FrameBuffer;
using dungeon::display::tui::Layout;
using dungeon::display::tui::make_layout;

DUNGEON_TEST(display, ui_layout_places_game_stats_and_inventory)
{
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

DUNGEON_TEST(display, ui_layout_is_empty_on_a_small_framebuffer)
{
    const Layout layout = make_layout(30, 10);

    CHECK_EQ(layout.game.width, 0U);
    CHECK_EQ(layout.game.height, 0U);
    CHECK_EQ(layout.stats.width, 0U);
    CHECK_EQ(layout.stats.height, 0U);
    CHECK_EQ(layout.inventory.width, 0U);
    CHECK_EQ(layout.inventory.height, 0U);
}

DUNGEON_TEST(display, ui_draws_a_generated_map)
{
    FrameBuffer buffer(100, 30);

    dungeon::map::Map map;
    map.generate(74, 24);

    dungeon::display::tui::draw_ui(buffer, map, make_layout(buffer.width(), buffer.height()));
}
