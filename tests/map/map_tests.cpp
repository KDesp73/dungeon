#include "test_harness.hpp"

#include "dungeon/map/map.hpp"

#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using dungeon::map::DoorSide;
using dungeon::map::Map;
using dungeon::map::MapSpec;
using dungeon::map::Point;
using dungeon::map::RoomSpec;
using dungeon::map::Tile;
using dungeon::map::entities::EnemyKind;
using dungeon::map::entities::ItemKind;

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

bool has_entity_at(const Map& map, char tile, std::size_t x, std::size_t y)
{
    for (const dungeon::map::entities::Entity& entity : map.entities()) {
        if (entity.tile() == tile && entity.x == x && entity.y == y) {
            return true;
        }
    }
    return false;
}

char item_glyph(ItemKind kind)
{
    switch (kind) {
    case ItemKind::Coin:
    case ItemKind::CoinDouble:
        return '$';
    case ItemKind::Diamond:
        return 'V';
    case ItemKind::Key:
        return 'k';
    case ItemKind::Chest:
        return '?';
    }
    return '?';
}

Point door_position(const RoomSpec& room, DoorSide side)
{
    std::size_t x = room.x + room.width / 2;
    std::size_t y = room.y + room.height / 2;

    switch (side) {
    case DoorSide::Top:
        y = room.y;
        break;
    case DoorSide::Right:
        x = room.x + room.width - 1;
        break;
    case DoorSide::Bottom:
        y = room.y + room.height - 1;
        break;
    case DoorSide::Left:
        x = room.x;
        break;
    }

    return Point{x, y};
}

bool path_reachable(const Map& map, Point start, Point goal)
{
    if (start.x >= map.width() || start.y >= map.height() || goal.x >= map.width() ||
        goal.y >= map.height()) {
        return false;
    }

    constexpr int offset_x[4] = {0, 1, 0, -1};
    constexpr int offset_y[4] = {-1, 0, 1, 0};

    std::vector<Point> pending{start};
    std::set<std::pair<std::size_t, std::size_t>> seen{{start.x, start.y}};

    while (!pending.empty()) {
        const Point point = pending.back();
        pending.pop_back();

        if (point.x == goal.x && point.y == goal.y) {
            return true;
        }

        for (int move = 0; move < 4; ++move) {
            const int next_x = static_cast<int>(point.x) + offset_x[move];
            const int next_y = static_cast<int>(point.y) + offset_y[move];

            if (next_x < 0 || next_y < 0 || next_x >= static_cast<int>(map.width()) ||
                next_y >= static_cast<int>(map.height())) {
                continue;
            }

            const auto cell =
                std::make_pair(static_cast<std::size_t>(next_x), static_cast<std::size_t>(next_y));

            if (!seen.insert(cell).second) {
                continue;
            }

            const Tile tile = map.at(cell.first, cell.second);
            if (tile == Tile::Walkway || tile == Tile::Door) {
                pending.push_back(Point{cell.first, cell.second});
            }
        }
    }

    return false;
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

DUNGEON_TEST(map, a_seed_reproduces_the_same_dungeon)
{
    Map first(12345);
    Map second(12345);

    first.generate();
    second.generate();

    CHECK_EQ(first.seed(), 12345U);
    CHECK_EQ(second.seed(), 12345U);
    CHECK(snapshot(first) == snapshot(second));
}

DUNGEON_TEST(map, reseeding_restarts_the_random_sequence)
{
    Map map(1);
    const std::string first = [&map] {
        map.generate();
        return snapshot(map);
    }();

    map.reseed(1);
    map.generate();

    CHECK(snapshot(map) == first);

    map.reseed(2);
    map.generate();

    CHECK(snapshot(map) != first);
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

DUNGEON_TEST(map, walkways_use_orthogonal_connections)
{
    MapSpec spec;
    spec.width  = 20;
    spec.height = 12;
    spec.rooms  = {{2, 2, 6, 5}, {12, 6, 6, 5}};
    spec.connections = {{0, 1, DoorSide::Right, DoorSide::Left}};

    Map map;
    map.generate(spec);

    const Point from = door_position(spec.rooms[0], DoorSide::Right);
    const Point to   = door_position(spec.rooms[1], DoorSide::Left);

    CHECK(map.at(from.x, from.y) == Tile::Door);
    CHECK(map.at(to.x, to.y) == Tile::Door);
    CHECK(path_reachable(map, from, to));
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

DUNGEON_TEST(map, walkways_connect_every_door_orthogonally)
{
    for (int run = 0; run < 30; ++run) {
        Map map;
        const MapSpec spec = map.make_spec();
        map.generate(spec);

        for (const dungeon::map::ConnectionSpec& connection : spec.connections) {
            const Point from = door_position(spec.rooms[connection.from], connection.from_side);
            const Point to   = door_position(spec.rooms[connection.to], connection.to_side);

            CHECK(path_reachable(map, from, to));
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

DUNGEON_TEST(map, make_spec_places_rooms_on_a_three_by_three_grid)
{
    Map map;

    for (int run = 0; run < 50; ++run) {
        const MapSpec spec = map.make_spec();
        REQUIRE(spec.rooms.size() >= 3);
        CHECK(spec.rooms.size() <= 9);

        const std::size_t cell_width  = (spec.width - 4) / 3;
        const std::size_t cell_height = (spec.height - 4) / 3;
        REQUIRE(cell_width > 0);
        REQUIRE(cell_height > 0);

        std::set<std::pair<std::size_t, std::size_t>> cells;
        for (const RoomSpec& room : spec.rooms) {
            const std::size_t center_x = room.x + room.width / 2;
            const std::size_t center_y = room.y + room.height / 2;
            const std::size_t column   = (center_x - 2) / cell_width;
            const std::size_t row      = (center_y - 2) / cell_height;

            CHECK(column < 3);
            CHECK(row < 3);
            CHECK(cells.insert({row, column}).second);
        }
    }
}

DUNGEON_TEST(map, make_spec_places_entrance_and_exit_inside_rooms)
{
    Map map;

    for (int run = 0; run < 30; ++run) {
        const MapSpec spec = map.make_spec();
        REQUIRE(!spec.rooms.empty());

        bool entrance_in_room = false;
        bool exit_in_room     = false;

        for (const dungeon::map::RoomSpec& room : spec.rooms) {
            entrance_in_room =
                entrance_in_room || inside_room(room, spec.entrance.x, spec.entrance.y);
            exit_in_room = exit_in_room || inside_room(room, spec.exit.x, spec.exit.y);
        }

        CHECK(entrance_in_room);
        CHECK(exit_in_room);
        CHECK(!(spec.entrance.x == spec.exit.x && spec.entrance.y == spec.exit.y));
    }
}

DUNGEON_TEST(map, generate_places_the_player_at_the_entrance)
{
    Map map;
    const MapSpec spec = map.make_spec(80, 40);

    map.generate(spec);

    CHECK(map.at(spec.entrance.x, spec.entrance.y) == Tile::Floor);
    CHECK(has_entity_at(map, '@', spec.entrance.x, spec.entrance.y));
    CHECK_EQ(map.entrance().x, spec.entrance.x);
    CHECK_EQ(map.entrance().y, spec.entrance.y);
    CHECK_EQ(map.exit().x, spec.exit.x);
    CHECK_EQ(map.exit().y, spec.exit.y);
}

DUNGEON_TEST(map, make_spec_spawns_items_inside_rooms)
{
    Map map;
    std::size_t total_items = 0;

    for (int run = 0; run < 30; ++run) {
        const MapSpec spec = map.make_spec();
        std::set<std::pair<std::size_t, std::size_t>> occupied;

        for (const dungeon::map::ItemSpec& item : spec.items) {
            ++total_items;

            bool inside = false;
            for (const dungeon::map::RoomSpec& room : spec.rooms) {
                inside = inside || inside_room(room, item.x, item.y);
            }

            CHECK(inside);
            CHECK(!(item.x == spec.entrance.x && item.y == spec.entrance.y));
            CHECK(!(item.x == spec.exit.x && item.y == spec.exit.y));
            CHECK(occupied.insert({item.x, item.y}).second);
        }
    }

    CHECK(total_items > 0);
}

DUNGEON_TEST(map, make_spec_places_exactly_one_key)
{
    Map map;

    for (int run = 0; run < 50; ++run) {
        const MapSpec spec = map.make_spec();
        std::size_t      key_count = 0;

        for (const dungeon::map::ItemSpec& item : spec.items) {
            if (item.kind == ItemKind::Key) {
                ++key_count;
            }
        }

        CHECK_EQ(key_count, 1U);
    }
}

DUNGEON_TEST(map, make_spec_spawns_enemies_inside_rooms)
{
    Map map;
    std::size_t total_enemies = 0;

    for (int run = 0; run < 30; ++run) {
        const MapSpec spec = map.make_spec();
        std::set<std::pair<std::size_t, std::size_t>> occupied;

        for (const dungeon::map::ItemSpec& item : spec.items) {
            static_cast<void>(occupied.insert({item.x, item.y}));
        }

        for (const dungeon::map::EnemySpec& enemy : spec.enemies) {
            ++total_enemies;

            bool inside = false;
            for (const dungeon::map::RoomSpec& room : spec.rooms) {
                inside = inside || inside_room(room, enemy.x, enemy.y);
            }

            CHECK(inside);
            CHECK(!(enemy.x == spec.entrance.x && enemy.y == spec.entrance.y));
            CHECK(!(enemy.x == spec.exit.x && enemy.y == spec.exit.y));
            CHECK(occupied.insert({enemy.x, enemy.y}).second);
        }
    }

    CHECK(total_enemies > 0);
}

DUNGEON_TEST(map, generate_places_enemies_from_the_spec)
{
    MapSpec spec;
    spec.width    = 24;
    spec.height   = 12;
    spec.rooms    = {{2, 2, 20, 8}};
    spec.entrance = {3, 3};
    spec.exit     = {20, 8};
    spec.enemies  = {{EnemyKind::Fast, 6, 4}, {EnemyKind::Normal, 9, 4},
                     {EnemyKind::Brute, 12, 4}};

    Map map;
    map.generate(spec);

    CHECK(has_entity_at(map, 'r', 6, 4));
    CHECK(has_entity_at(map, 'g', 9, 4));
    CHECK(has_entity_at(map, 'O', 12, 4));

    std::size_t enemy_count = 0;
    for (const dungeon::map::EntityPtr& entity : map.entity_ptrs()) {
        if (dynamic_cast<const dungeon::map::entities::Enemy*>(entity.get()) != nullptr) {
            ++enemy_count;
        }
    }

    CHECK_EQ(enemy_count, spec.enemies.size());
}

DUNGEON_TEST(map, enemy_archetypes_trade_speed_for_durability)
{
    const auto& fast   = dungeon::map::entities::enemy_profile(EnemyKind::Fast);
    const auto& normal = dungeon::map::entities::enemy_profile(EnemyKind::Normal);
    const auto& brute  = dungeon::map::entities::enemy_profile(EnemyKind::Brute);

    CHECK(fast.max_hp < normal.max_hp);
    CHECK(normal.max_hp < brute.max_hp);
    CHECK(fast.attack < normal.attack);
    CHECK(normal.attack < brute.attack);
    CHECK(fast.move_interval < normal.move_interval);
    CHECK(normal.move_interval < brute.move_interval);
    CHECK(fast.gold_max < brute.gold_min);
    CHECK(fast.glyph != normal.glyph);
    CHECK(normal.glyph != brute.glyph);
    CHECK(fast.glyph != brute.glyph);
}

DUNGEON_TEST(map, generate_places_items_from_the_spec)
{
    Map source;
    const MapSpec spec = source.make_spec(80, 40);

    Map map;
    map.generate(spec);

    CHECK_EQ(map.entities().size(), spec.items.size() + spec.enemies.size() + 1);

    std::size_t key_count = 0;
    for (const dungeon::map::entities::Entity& entity : map.entities()) {
        if (entity.tile() == 'k') {
            ++key_count;
        }
    }
    CHECK_EQ(key_count, 1U);

    for (const dungeon::map::ItemSpec& item : spec.items) {
        CHECK(has_entity_at(map, item_glyph(item.kind), item.x, item.y));
    }
}

DUNGEON_TEST(map, generate_builds_chest_contents_from_the_spec)
{
    MapSpec spec;
    spec.width    = 20;
    spec.height   = 10;
    spec.rooms    = {{2, 2, 6, 5}};
    spec.entrance = {3, 3};
    spec.items    = {{ItemKind::Chest, 4, 3, {ItemKind::Coin, ItemKind::CoinDouble}}};

    Map map;
    map.generate(spec);

    const dungeon::map::entities::Chest* chest = nullptr;
    for (const dungeon::map::EntityPtr& entity : map.entity_ptrs()) {
        if (const auto* candidate =
                dynamic_cast<const dungeon::map::entities::Chest*>(entity.get())) {
            chest = candidate;
        }
    }

    REQUIRE(chest != nullptr);
    REQUIRE(chest->contents().size() == 2U);
    CHECK(chest->contents()[0].kind() == ItemKind::Coin);
    CHECK(chest->contents()[1].kind() == ItemKind::CoinDouble);
}

DUNGEON_TEST(map, generate_builds_a_diamond_from_the_spec)
{
    MapSpec spec;
    spec.width    = 20;
    spec.height   = 10;
    spec.rooms    = {{2, 2, 6, 5}};
    spec.entrance = {3, 3};
    spec.items    = {{ItemKind::Diamond, 4, 3}};

    Map map;
    map.generate(spec);

    CHECK(has_entity_at(map, 'V', 4, 3));

    const dungeon::map::entities::Diamond* diamond = nullptr;
    for (const dungeon::map::EntityPtr& entity : map.entity_ptrs()) {
        if (const auto* candidate =
                dynamic_cast<const dungeon::map::entities::Diamond*>(entity.get())) {
            diamond = candidate;
        }
    }

    REQUIRE(diamond != nullptr);
    CHECK_EQ(diamond->gold(), 10);
}

DUNGEON_TEST(map, make_spec_rolls_loot_for_every_chest)
{
    Map map;

    for (int run = 0; run < 100; ++run) {
        const MapSpec spec = map.make_spec();

        for (const dungeon::map::ItemSpec& item : spec.items) {
            if (item.kind == ItemKind::Chest) {
                CHECK(!item.contents.empty());
            }
        }
    }
}

DUNGEON_TEST(map, clear_removes_entities)
{
    Map map;
    map.generate();

    CHECK(!map.entities().empty());

    map.clear();

    CHECK(map.entities().empty());
    CHECK_EQ(map.entrance().x, 0U);
    CHECK_EQ(map.entrance().y, 0U);
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
    map.set(2, 1, Tile::Floor);
    map.set(2, 2, Tile::Door);

    map.print(file);

    std::rewind(file);
    char buffer[128] = {};
    const std::size_t bytes = std::fread(buffer, 1, sizeof(buffer) - 1, file);
    std::fclose(file);

    CHECK_EQ(std::string(buffer, bytes), std::string("-----\n| . |\n| + |\n-----\n"));
}

DUNGEON_TEST(map, generate_makes_the_player_directly_available)
{
    Map map;
    const MapSpec spec = map.make_spec(80, 40);

    map.generate(spec);

    REQUIRE(map.player() != nullptr);
    CHECK_EQ(map.player()->x, spec.entrance.x);
    CHECK_EQ(map.player()->y, spec.entrance.y);
}

DUNGEON_TEST(map, removing_the_player_clears_the_cached_handle)
{
    Map map;
    const MapSpec spec = map.make_spec(80, 40);
    map.generate(spec);

    REQUIRE(map.player() != nullptr);
    const int player_id = map.player()->id();

    CHECK(map.remove(player_id));
    CHECK(map.player() == nullptr);
    CHECK(!map.remove(player_id));
}

DUNGEON_TEST(map, walkable_accepts_floor_walkway_and_doors)
{
    Map map;
    map.generate(5, 4);

    map.set(1, 1, Tile::Floor);
    map.set(2, 1, Tile::Walkway);
    map.set(3, 1, Tile::Door);
    map.set(1, 2, Tile::HWall);
    map.set(2, 2, Tile::VWall);

    CHECK(map.walkable(1, 1));
    CHECK(map.walkable(2, 1));
    CHECK(map.walkable(3, 1));
    CHECK(!map.walkable(1, 2));
    CHECK(!map.walkable(2, 2));
    CHECK(!map.walkable(0, 0));
    CHECK(!map.walkable(5, 0));
    CHECK(!map.walkable(0, 4));
}
