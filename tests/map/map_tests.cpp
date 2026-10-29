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

DUNGEON_TEST(map, generate_builds_a_bordered_room)
{
    Map map;
    map.generate(5, 4);

    CHECK_EQ(map.width(), 5U);
    CHECK_EQ(map.height(), 4U);
    CHECK(!map.empty());

    for (std::size_t x = 0; x < map.width(); ++x) {
        CHECK(map.at(x, 0) == Tile::Wall);
        CHECK(map.at(x, map.height() - 1) == Tile::Wall);
    }

    for (std::size_t y = 0; y < map.height(); ++y) {
        CHECK(map.at(0, y) == Tile::Wall);
        CHECK(map.at(map.width() - 1, y) == Tile::Wall);
    }

    for (std::size_t y = 1; y < map.height() - 1; ++y) {
        for (std::size_t x = 1; x < map.width() - 1; ++x) {
            CHECK(map.at(x, y) == Tile::Floor);
        }
    }
}

DUNGEON_TEST(map, generate_defaults_to_32_by_32)
{
    Map map;
    map.generate();

    CHECK_EQ(map.width(), 32U);
    CHECK_EQ(map.height(), 32U);
    CHECK(map.at(0, 0) == Tile::Wall);
    CHECK(map.at(1, 1) == Tile::Floor);
}

DUNGEON_TEST(map, generate_replaces_existing_content)
{
    Map map;
    map.generate(3, 3);
    map.set(1, 1, Tile::Wall);

    map.generate(5, 5);

    CHECK_EQ(map.width(), 5U);
    CHECK_EQ(map.height(), 5U);
    CHECK(map.at(1, 1) == Tile::Floor);
}

DUNGEON_TEST(map, generate_with_a_zero_dimension_clears_the_map)
{
    Map map;
    map.generate(4, 4);

    map.generate(0, 4);

    CHECK(map.empty());
    CHECK_EQ(map.width(), 0U);
    CHECK_EQ(map.height(), 0U);
}

DUNGEON_TEST(map, set_updates_a_tile)
{
    Map map;
    map.generate(4, 4);

    map.set(1, 2, Tile::Wall);

    CHECK(map.at(1, 2) == Tile::Wall);
    CHECK(map.at(2, 1) == Tile::Floor);
}

DUNGEON_TEST(map, coordinates_outside_the_map_are_rejected)
{
    Map map;
    map.generate(4, 4);

    CHECK_THROWS_AS((void)map.at(4, 0), std::out_of_range);
    CHECK_THROWS_AS((void)map.at(0, 4), std::out_of_range);
    CHECK_THROWS_AS(map.set(4, 0, Tile::Wall), std::out_of_range);
    CHECK_THROWS_AS(map.set(0, 4, Tile::Wall), std::out_of_range);
}

DUNGEON_TEST(map, tiles_have_ascii_glyphs)
{
    CHECK_EQ(dungeon::map::glyph(Tile::Wall), '#');
    CHECK_EQ(dungeon::map::glyph(Tile::Floor), '.');
}

DUNGEON_TEST(map, print_writes_ascii_rows)
{
    FILE* file = std::tmpfile();
    REQUIRE(file != nullptr);

    Map map;
    map.generate(3, 3);
    map.print(file);

    std::rewind(file);
    char buffer[16] = {};
    const std::size_t bytes = std::fread(buffer, 1, sizeof(buffer) - 1, file);
    std::fclose(file);

    CHECK_EQ(std::string(buffer, bytes), std::string("###\n#.#\n###\n"));
}
