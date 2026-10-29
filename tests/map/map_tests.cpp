#include "test_harness.hpp"

#include "dungeon/map/map.hpp"

#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>

using dungeon::map::DoorSide;
using dungeon::map::Map;
using dungeon::map::MapSpec;
using dungeon::map::Tile;

namespace {

std::string snapshot(const Map& map)
{
    std::string result;
    result.reserve(map.width() * map.height());

    for (std::size_t y = 0; y < map.height(); ++y) {
        for (std::size_t x = 0; x < map.width(); ++x) {
            result.push_back(dungeon::map::glyph(map.at(x, y)));
        }
    }

    return result;
}

std::size_t count_tiles(const Map& map, Tile tile)
{
    std::size_t count = 0;

    for (std::size_t y = 0; y < map.height(); ++y) {
        for (std::size_t x = 0; x < map.width(); ++x) {
            if (map.at(x, y) == tile) {
                ++count;
            }
        }
    }

    return count;
}

bool inside_room(const dungeon::map::RoomSpec& room, std::size_t x, std::size_t y)
{
    return x >= room.x && x < room.x + room.width && y >= room.y &&
           y < room.y + room.height;
}

bool is_path(Tile tile)
{
    return tile == Tile::Walkway || tile == Tile::Door;
}

}  // namespace

DUNGEON_TEST(map, a_new_map_is_empty)
{
    Map map;
    CHECK(map.empty());
    CHECK_EQ(map.width(), 0U);
    CHECK_EQ(map.height(), 0U);
}

DUNGEON_TEST(map, seed_stays_the_same_for_a_map_instance)
{
    Map map;
    const auto seed = map.seed();

    map.generate();

    CHECK_EQ(map.seed(), seed);
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

    CHECK(count_tiles(map, Tile::HWall) > 0);
    CHECK(count_tiles(map, Tile::VWall) > 0);
    CHECK(count_tiles(map, Tile::Door) >= 2);
    CHECK(count_tiles(map, Tile::Walkway) > 0);
    CHECK(count_tiles(map, Tile::Floor) > 0);
}

DUNGEON_TEST(map, generate_keeps_rooms_inside_the_margin)
{
    Map map;
    map.generate();

    for (std::size_t x = 0; x < map.width(); ++x) {
        for (std::size_t y = 0; y < 2; ++y) {
            CHECK(map.at(x, y) == Tile::Empty);
            CHECK(map.at(x, map.height() - 1 - y) == Tile::Empty);
        }
    }

    for (std::size_t y = 0; y < map.height(); ++y) {
        for (std::size_t x = 0; x < 2; ++x) {
            CHECK(map.at(x, y) == Tile::Empty);
            CHECK(map.at(map.width() - 1 - x, y) == Tile::Empty);
        }
    }
}

DUNGEON_TEST(map, generate_connects_doors_with_a_walkway)
{
    Map map;
    map.generate();

    const std::size_t doors = count_tiles(map, Tile::Door);
    CHECK(doors >= 2);

    for (std::size_t y = 0; y < map.height(); ++y) {
        for (std::size_t x = 0; x < map.width(); ++x) {
            if (map.at(x, y) != Tile::Door) {
                continue;
            }

            bool touches_walkway = false;
            const std::size_t min_x = x > 0 ? x - 1 : 0;
            const std::size_t min_y = y > 0 ? y - 1 : 0;
            const std::size_t max_x = x + 1 < map.width() ? x + 1 : x;
            const std::size_t max_y = y + 1 < map.height() ? y + 1 : y;

            for (std::size_t ny = min_y; ny <= max_y; ++ny) {
                for (std::size_t nx = min_x; nx <= max_x; ++nx) {
                    if ((nx != x || ny != y) && map.at(nx, ny) == Tile::Walkway) {
                        touches_walkway = true;
                    }
                }
            }

            CHECK(touches_walkway);
        }
    }
}

DUNGEON_TEST(map, walkways_never_cross_rooms)
{
    for (int run = 0; run < 30; ++run) {
        Map map;
        const MapSpec spec = map.make_spec();
        map.generate(spec);

        for (std::size_t y = 0; y < map.height(); ++y) {
            for (std::size_t x = 0; x < map.width(); ++x) {
                if (map.at(x, y) != Tile::Walkway) {
                    continue;
                }

                for (const dungeon::map::RoomSpec& room : spec.rooms) {
                    CHECK(!inside_room(room, x, y));
                }
            }
        }
    }
}

DUNGEON_TEST(map, walkways_prefer_direct_diagonal_routes)
{
    MapSpec spec;
    spec.width  = 20;
    spec.height = 12;
    spec.rooms  = {{2, 2, 6, 5}, {12, 6, 6, 5}};
    spec.connections = {{0, 1, DoorSide::Right, DoorSide::Left}};

    Map map;
    map.generate(spec);

    CHECK(map.at(7, 4) == Tile::Door);
    CHECK(map.at(12, 8) == Tile::Door);

    // The original Bresenham line from (7, 4) to (12, 8).
    CHECK(map.at(8, 5) == Tile::Walkway);
    CHECK(map.at(9, 6) == Tile::Walkway);
    CHECK(map.at(10, 6) == Tile::Walkway);
    CHECK(map.at(11, 7) == Tile::Walkway);

    // The diagonal steps are kept, but one shared corner per step is filled so
    // the path never leaves two diagonally adjacent floor tiles.
    CHECK(map.at(8, 4) == Tile::Walkway);
    CHECK(map.at(9, 5) == Tile::Walkway);
    CHECK(map.at(11, 6) == Tile::Walkway);
    CHECK(map.at(11, 8) == Tile::Walkway);
    CHECK_EQ(count_tiles(map, Tile::Walkway), 8U);
}

DUNGEON_TEST(map, walkways_detour_around_blocking_rooms)
{
    MapSpec spec;
    spec.width  = 24;
    spec.height = 10;
    spec.rooms  = {
        {2, 3, 5, 4},
        {9, 4, 6, 3},
        {18, 3, 5, 4},
    };
    spec.connections = {{0, 2, DoorSide::Right, DoorSide::Left}};

    Map map;
    map.generate(spec);

    CHECK(map.at(6, 5) == Tile::Door);
    CHECK(map.at(18, 5) == Tile::Door);
    CHECK(count_tiles(map, Tile::Walkway) > 0);

    for (std::size_t y = 0; y < map.height(); ++y) {
        for (std::size_t x = 0; x < map.width(); ++x) {
            if (map.at(x, y) != Tile::Walkway) {
                continue;
            }

            for (const dungeon::map::RoomSpec& room : spec.rooms) {
                CHECK(!inside_room(room, x, y));
            }
        }
    }
}

DUNGEON_TEST(map, walkway_diagonals_have_no_open_corners)
{
    for (int run = 0; run < 30; ++run) {
        Map map;
        map.generate();

        for (std::size_t y = 0; y < map.height(); ++y) {
            for (std::size_t x = 0; x < map.width(); ++x) {
                if (!is_path(map.at(x, y))) {
                    continue;
                }

                for (int offset_y = -1; offset_y <= 1; offset_y += 2) {
                    for (int offset_x = -1; offset_x <= 1; offset_x += 2) {
                        const int diagonal_x = static_cast<int>(x) + offset_x;
                        const int diagonal_y = static_cast<int>(y) + offset_y;

                        if (diagonal_x < 0 || diagonal_y < 0 ||
                            diagonal_x >= static_cast<int>(map.width()) ||
                            diagonal_y >= static_cast<int>(map.height())) {
                            continue;
                        }

                        const std::size_t next_x = static_cast<std::size_t>(diagonal_x);
                        const std::size_t next_y = static_cast<std::size_t>(diagonal_y);

                        if (!is_path(map.at(next_x, next_y))) {
                            continue;
                        }

                        const bool corner_a_is_floor = map.at(next_x, y) == Tile::Empty;
                        const bool corner_b_is_floor = map.at(x, next_y) == Tile::Empty;

                        CHECK(!(corner_a_is_floor && corner_b_is_floor));
                    }
                }
            }
        }
    }
}

DUNGEON_TEST(map, generate_produces_a_different_dungeon_each_call)
{
    Map first;
    Map second;

    first.generate();
    second.generate();

    CHECK(snapshot(first) != snapshot(second));
}

DUNGEON_TEST(map, generate_varies_room_layout_and_count)
{
    Map map;
    std::set<std::size_t> door_counts;
    std::set<std::string> layouts;

    for (int run = 0; run < 30; ++run) {
        map.generate();
        door_counts.insert(count_tiles(map, Tile::Door));
        layouts.insert(snapshot(map));
    }

    CHECK(door_counts.size() > 1);
    CHECK(layouts.size() > 1);
}

DUNGEON_TEST(map, make_spec_describes_the_generated_dungeon)
{
    Map map;
    const MapSpec spec = map.make_spec(80, 40);

    CHECK_EQ(spec.width, 80U);
    CHECK_EQ(spec.height, 40U);
    CHECK(spec.rooms.size() >= 2);
    CHECK_EQ(spec.connections.size(), spec.rooms.size() - 1);

    for (const dungeon::map::ConnectionSpec& connection : spec.connections) {
        CHECK(connection.from < spec.rooms.size());
        CHECK(connection.to < spec.rooms.size());
    }
}

DUNGEON_TEST(map, generate_recreates_a_spec_exactly)
{
    Map random_map;
    const MapSpec spec = random_map.make_spec();

    Map first;
    first.generate(spec);

    Map second;
    second.generate(spec);

    CHECK_EQ(first.width(), spec.width);
    CHECK_EQ(first.height(), spec.height);
    CHECK(snapshot(first) == snapshot(second));
}

DUNGEON_TEST(map, generate_builds_an_explicit_spec)
{
    MapSpec spec;
    spec.width  = 20;
    spec.height = 10;
    spec.rooms  = {{2, 2, 6, 5}, {12, 2, 6, 5}};
    spec.connections = {{0, 1, DoorSide::Right, DoorSide::Left}};

    Map map;
    map.generate(spec);

    CHECK_EQ(map.width(), 20U);
    CHECK_EQ(map.height(), 10U);

    CHECK(map.at(2, 2) == Tile::HWall);
    CHECK(map.at(7, 2) == Tile::HWall);
    CHECK(map.at(2, 6) == Tile::HWall);
    CHECK(map.at(17, 6) == Tile::HWall);

    CHECK(map.at(3, 3) == Tile::Floor);
    CHECK(map.at(13, 3) == Tile::Floor);

    CHECK(map.at(7, 4) == Tile::Door);
    CHECK(map.at(12, 4) == Tile::Door);

    for (std::size_t x = 8; x < 12; ++x) {
        CHECK(map.at(x, 4) == Tile::Walkway);
    }
}

DUNGEON_TEST(map, generate_rejects_connections_to_unknown_rooms)
{
    MapSpec spec;
    spec.width  = 10;
    spec.height = 10;
    spec.rooms  = {{2, 2, 4, 4}};
    spec.connections = {{0, 1, DoorSide::Right, DoorSide::Left}};

    Map map;
    CHECK_THROWS_AS(map.generate(spec), std::out_of_range);
}

DUNGEON_TEST(map, generate_skips_rooms_when_the_map_is_too_small)
{
    Map map;
    map.generate(5, 4);

    CHECK_EQ(map.width(), 5U);
    CHECK_EQ(map.height(), 4U);

    for (std::size_t y = 0; y < map.height(); ++y) {
        for (std::size_t x = 0; x < map.width(); ++x) {
            CHECK(map.at(x, y) == Tile::Empty);
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
    CHECK(map.at(0, 0) == Tile::Empty);
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
    map.generate(5, 4);

    map.set(2, 2, Tile::Door);

    CHECK(map.at(2, 2) == Tile::Door);
    CHECK(map.at(3, 2) == Tile::Empty);
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
    CHECK_EQ(dungeon::map::glyph(Tile::Empty), ' ');
    CHECK_EQ(dungeon::map::glyph(Tile::Player), '@');
    CHECK_EQ(dungeon::map::glyph(Tile::Key), 'K');
    CHECK_EQ(dungeon::map::glyph(Tile::Door), '+');
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

    CHECK_EQ(std::string(buffer, bytes), std::string("-----\n| @ |\n| + |\n-----\n"));
}
