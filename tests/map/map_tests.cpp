#include "test_harness.hpp"

#include "dungeon/map/map.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

using dungeon::map::Map;
using dungeon::map::Tile;

DUNGEON_TEST(map, a_new_map_is_empty)
{
    Map map;
    CHECK(map.empty());
    CHECK_EQ(map.width(), 0U);
    CHECK_EQ(map.height(), 0U);
}

DUNGEON_TEST(map, generate_defaults_to_64_by_32)
{
    Map map;
    map.generate();

    CHECK_EQ(map.width(), 64U);
    CHECK_EQ(map.height(), 32U);
    CHECK(!map.empty());
}

DUNGEON_TEST(map, generate_creates_rooms_with_doors)
{
    Map map;
    map.generate();

    constexpr std::size_t left_x   = 2;
    constexpr std::size_t left_y   = 11;
    constexpr std::size_t right_x  = 46;
    constexpr std::size_t room_w   = 16;
    constexpr std::size_t room_h   = 10;
    constexpr std::size_t room_bot = left_y + room_h - 1;
    constexpr std::size_t door_y   = left_y + room_h / 2;

    for (std::size_t x = left_x; x < left_x + room_w; ++x) {
        CHECK(map.at(x, left_y) == Tile::HWall);
        CHECK(map.at(x, room_bot) == Tile::HWall);
    }
    for (std::size_t x = right_x; x < right_x + room_w; ++x) {
        CHECK(map.at(x, left_y) == Tile::HWall);
        CHECK(map.at(x, room_bot) == Tile::HWall);
    }

    for (std::size_t y = left_y + 1; y < room_bot; ++y) {
        CHECK(map.at(left_x, y) == Tile::VWall);
        CHECK(map.at(right_x + room_w - 1, y) == Tile::VWall);
    }

    CHECK(map.at(left_x + room_w - 1, door_y) == Tile::Door);
    CHECK(map.at(right_x, door_y) == Tile::Door);

    CHECK(map.at(left_x + 1, left_y + 1) == Tile::Floor);
    CHECK(map.at(right_x + 1, left_y + 1) == Tile::Floor);
}

DUNGEON_TEST(map, generate_connects_doors_with_a_walkway)
{
    Map map;
    map.generate();

    constexpr std::size_t door_y     = 16;
    constexpr std::size_t left_door  = 17;
    constexpr std::size_t right_door = 46;

    CHECK(map.at(left_door, door_y) == Tile::Door);
    CHECK(map.at(right_door, door_y) == Tile::Door);

    for (std::size_t x = left_door + 1; x < right_door; ++x) {
        CHECK(map.at(x, door_y) == Tile::Walkway);
    }

    CHECK(map.at(left_door - 1, door_y) == Tile::Floor);
    CHECK(map.at(right_door + 1, door_y) == Tile::Floor);
}

DUNGEON_TEST(map, generate_skips_rooms_when_the_map_is_too_small)
{
    Map map;
    map.generate(5, 4);

    CHECK_EQ(map.width(), 5U);
    CHECK_EQ(map.height(), 4U);

    for (std::size_t y = 0; y < map.height(); ++y) {
        for (std::size_t x = 0; x < map.width(); ++x) {
            CHECK(map.at(x, y) == Tile::Floor);
        }
    }
}

DUNGEON_TEST(map, generate_replaces_existing_content)
{
    Map map;
    map.generate();
    map.set(0, 0, Tile::Door);

    map.generate();

    CHECK_EQ(map.width(), 64U);
    CHECK_EQ(map.height(), 32U);
    CHECK(map.at(0, 0) == Tile::Floor);
}

DUNGEON_TEST(map, generate_with_a_zero_dimension_clears_the_map)
{
    Map map;
    map.generate();

    map.generate(0, 4);

    CHECK(map.empty());
    CHECK_EQ(map.width(), 0U);
    CHECK_EQ(map.height(), 0U);
}

DUNGEON_TEST(map, set_updates_a_tile)
{
    Map map;
    map.generate();

    map.set(20, 20, Tile::Door);

    CHECK(map.at(20, 20) == Tile::Door);
    CHECK(map.at(21, 20) == Tile::Floor);
}

DUNGEON_TEST(map, coordinates_outside_the_map_are_rejected)
{
    Map map;
    map.generate(5, 4);

    CHECK_THROWS_AS((void)map.at(5, 0), std::out_of_range);
    CHECK_THROWS_AS((void)map.at(0, 4), std::out_of_range);
    CHECK_THROWS_AS(map.set(5, 0, Tile::Door), std::out_of_range);
    CHECK_THROWS_AS(map.set(0, 4, Tile::Door), std::out_of_range);
}

DUNGEON_TEST(map, is_wall_recognises_wall_tiles)
{
    Map map;
    map.generate(5, 4);

    map.set(1, 1, Tile::HWall);
    map.set(2, 1, Tile::VWall);
    map.set(3, 1, Tile::Walkway);

    CHECK(map.is_wall(1, 1));
    CHECK(map.is_wall(2, 1));
    CHECK(!map.is_wall(3, 1));
    CHECK(!map.is_wall(0, 0));

    const std::size_t index = 1 * map.width() + 1;
    CHECK(map.is_wall(index));
}

DUNGEON_TEST(map, tiles_have_ascii_glyphs)
{
    CHECK_EQ(dungeon::map::glyph(Tile::HWall), '-');
    CHECK_EQ(dungeon::map::glyph(Tile::VWall), '|');
    CHECK_EQ(dungeon::map::glyph(Tile::Walkway), '#');
    CHECK_EQ(dungeon::map::glyph(Tile::Floor), '.');
    CHECK_EQ(dungeon::map::glyph(Tile::Player), '@');
    CHECK_EQ(dungeon::map::glyph(Tile::Key), 'K');
    CHECK_EQ(dungeon::map::glyph(Tile::Door), 'D');
}

DUNGEON_TEST(map, print_writes_ascii_rows)
{
    FILE* file = std::tmpfile();
    REQUIRE(file != nullptr);

    Map map;
    map.generate(5, 4);

    for (std::size_t x = 0; x < map.width(); ++x) {
        map.set(x, 0, Tile::HWall);
        map.set(x, map.height() - 1, Tile::HWall);
    }
    for (std::size_t y = 1; y < map.height() - 1; ++y) {
        map.set(0, y, Tile::VWall);
        map.set(map.width() - 1, y, Tile::VWall);
    }
    map.set(2, 1, Tile::Player);
    map.set(2, 2, Tile::Door);

    map.print(file);

    std::rewind(file);
    char buffer[128] = {};
    const std::size_t bytes = std::fread(buffer, 1, sizeof(buffer) - 1, file);
    std::fclose(file);

    CHECK_EQ(std::string(buffer, bytes), std::string("-----\n|.@.|\n|.D.|\n-----\n"));
}
