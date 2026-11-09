#include "dungeon/map/chance.hpp"
#include "dungeon/map/map.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <queue>
#include <random>
#include <stdexcept>

namespace dungeon::map {

namespace {

entities::Item make_loot_item(entities::ItemKind kind) {
    switch (kind) {
    case entities::ItemKind::Coin:
        return entities::Coin(0, 0);
    case entities::ItemKind::CoinDouble:
        return entities::CoinDouble(0, 0);
    case entities::ItemKind::Diamond:
        return entities::Diamond(0, 0);
    case entities::ItemKind::Key:
        return entities::Key(0, 0);
    case entities::ItemKind::Chest:
        break;
    }

    return entities::Coin(0, 0);
}

EntityPtr make_item_entity(const ItemSpec& item) {
    if (item.kind == entities::ItemKind::Chest) {
        auto chest = std::make_shared<entities::Chest>(item.x, item.y);

        for (const entities::ItemKind kind : item.contents) {
            chest->add(make_loot_item(kind));
        }

        return chest;
    }

    switch (item.kind) {
    case entities::ItemKind::Coin:
        return std::make_shared<entities::Coin>(item.x, item.y);
    case entities::ItemKind::CoinDouble:
        return std::make_shared<entities::CoinDouble>(item.x, item.y);
    case entities::ItemKind::Diamond:
        return std::make_shared<entities::Diamond>(item.x, item.y);
    case entities::ItemKind::Key:
        return std::make_shared<entities::Key>(item.x, item.y);
    case entities::ItemKind::Chest:
        break;
    }

    return std::make_shared<entities::Coin>(item.x, item.y);
}

EntityPtr make_enemy_entity(const EnemySpec& enemy) {
    return std::make_shared<entities::Enemy>(enemy.kind, enemy.x, enemy.y);
}

} // namespace

void Map::validate_room(std::size_t x, std::size_t y, std::size_t width, std::size_t height) const {
    if (width < 3 || height < 3) {
        throw std::invalid_argument("Map::room: room must be at least 3x3");
    }

    if (_width == 0 || _height == 0 || x >= _width || y >= _height || width > _width - x ||
        height > _height - y) {
        throw std::out_of_range("Map::room: room does not fit inside the map");
    }
}

void Map::room(std::size_t x, std::size_t y, std::size_t width, std::size_t height) {
    validate_room(x, y, width, height);

    const std::size_t right  = x + width - 1;
    const std::size_t bottom = y + height - 1;

    for (std::size_t row = y + 1; row < bottom; ++row) {
        for (std::size_t column = x + 1; column < right; ++column) {
            set(column, row, Tile::Floor);
        }
    }

    for (std::size_t column = x; column <= right; ++column) {
        set(column, y, Tile::HWall);
        set(column, bottom, Tile::HWall);
    }

    for (std::size_t row = y + 1; row < bottom; ++row) {
        set(x, row, Tile::VWall);
        set(right, row, Tile::VWall);
    }
}

Map::Door Map::door(std::size_t x, std::size_t y, std::size_t width, std::size_t height,
                    DoorSide side) {
    validate_room(x, y, width, height);

    const std::size_t right  = x + width - 1;
    const std::size_t bottom = y + height - 1;

    std::size_t door_x = x + width / 2;
    std::size_t door_y = y + height / 2;

    switch (side) {
    case DoorSide::Top:
        door_y = y;
        break;
    case DoorSide::Right:
        door_x = right;
        break;
    case DoorSide::Bottom:
        door_y = bottom;
        break;
    case DoorSide::Left:
        door_x = x;
        break;
    }

    set(door_x, door_y, Tile::Door);
    return Door{door_x, door_y};
}

void Map::walkway(std::size_t start_x, std::size_t start_y, std::size_t end_x, std::size_t end_y,
                  const std::vector<RoomSpec>& rooms) {
    if (_width == 0 || _height == 0 || start_x >= _width || start_y >= _height || end_x >= _width ||
        end_y >= _height) {
        throw std::out_of_range("Map::walkway: endpoint is outside the map");
    }

    const auto inside_room = [&rooms](std::size_t x, std::size_t y) {
        for (const RoomSpec& room : rooms) {
            if (x >= room.x && x < room.x + room.width && y >= room.y && y < room.y + room.height) {
                return true;
            }
        }
        return false;
    };

    const std::size_t start = start_y * _width + start_x;
    const std::size_t goal  = end_y * _width + end_x;

    if (start == goal) {
        return;
    }

    const auto is_blocked = [&](std::size_t index) {
        const std::size_t x = index % _width;
        const std::size_t y = index / _width;

        if (index == start || index == goal) {
            return false;
        }

        return inside_room(x, y) || is_wall(index);
    };

    // Four-way movement keeps every walkway horizontal or vertical. The turn
    // cost keeps the route close to a straight or L-shaped corridor instead of
    // zig-zagging through the open cells.
    constexpr std::size_t direction_count = 5; // 0 = start, 1..4 = directions
    constexpr std::size_t turn_cost       = 2;

    const std::size_t state_count = _tiles.size() * direction_count;
    const std::size_t infinity    = std::numeric_limits<std::size_t>::max();

    std::vector<std::size_t> best(state_count, infinity);
    std::vector<std::size_t> parent(state_count, infinity);

    struct QueueEntry {
        std::size_t f;
        std::size_t state;
    };

    struct QueueEntryGreater {
        bool operator()(const QueueEntry& lhs, const QueueEntry& rhs) const {
            return lhs.f > rhs.f;
        }
    };

    std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueEntryGreater> open;

    const auto heuristic = [&](std::size_t index) {
        const std::size_t x  = index % _width;
        const std::size_t y  = index / _width;
        const std::size_t dx = x > end_x ? x - end_x : end_x - x;
        const std::size_t dy = y > end_y ? y - end_y : end_y - y;
        return dx + dy;
    };

    const std::size_t start_state = start * direction_count;
    best[start_state]             = 0;
    open.push(QueueEntry{heuristic(start), start_state});

    std::size_t goal_state = infinity;

    constexpr int offset_x[4] = {0, 1, 0, -1};
    constexpr int offset_y[4] = {-1, 0, 1, 0};

    while (!open.empty()) {
        const QueueEntry entry = open.top();
        open.pop();

        const std::size_t state     = entry.state;
        const std::size_t index     = state / direction_count;
        const int         direction = static_cast<int>(state % direction_count);

        if (entry.f > best[state] + heuristic(index)) {
            continue;
        }

        if (index == goal) {
            goal_state = state;
            break;
        }

        const std::size_t x = index % _width;
        const std::size_t y = index / _width;

        for (int move = 0; move < 4; ++move) {
            const int next_x = static_cast<int>(x) + offset_x[move];
            const int next_y = static_cast<int>(y) + offset_y[move];

            if (next_x < 0 || next_y < 0 || next_x >= static_cast<int>(_width) ||
                next_y >= static_cast<int>(_height)) {
                continue;
            }

            const std::size_t next =
                static_cast<std::size_t>(next_y) * _width + static_cast<std::size_t>(next_x);

            if (is_blocked(next)) {
                continue;
            }

            const int   next_direction = move + 1;
            std::size_t cost           = best[state] + 1;

            if (direction != 0 && direction != next_direction) {
                cost += turn_cost;
            }

            const std::size_t next_state =
                next * direction_count + static_cast<std::size_t>(next_direction);

            if (cost < best[next_state]) {
                best[next_state]   = cost;
                parent[next_state] = state;
                open.push(QueueEntry{cost + heuristic(next), next_state});
            }
        }
    }

    if (goal_state == infinity) {
        throw std::runtime_error("Map::walkway: could not find a route between the doors");
    }

    std::size_t state = goal_state;

    while (true) {
        const std::size_t index = state / direction_count;
        const std::size_t x     = index % _width;
        const std::size_t y     = index / _width;

        if (at(x, y) != Tile::Door) {
            set(x, y, Tile::Walkway);
        }

        if (state == start_state) {
            break;
        }

        state = parent[state];
    }
}

MapSpec Map::make_spec(std::size_t width, std::size_t height) {
    MapSpec spec;
    spec.width  = width;
    spec.height = height;

    if (width == 0 || height == 0) {
        return spec;
    }

    constexpr std::size_t margin          = 2;
    constexpr std::size_t grid_columns    = 3;
    constexpr std::size_t grid_rows       = 3;
    constexpr std::size_t min_room_width  = 6;
    constexpr std::size_t max_room_width  = 14;
    constexpr std::size_t min_room_height = 4;
    constexpr std::size_t max_room_height = 8;
    constexpr std::size_t room_gutter     = 2;
    constexpr std::size_t min_rooms       = 3;

    if (width < margin * 2 + grid_columns * (min_room_width + room_gutter) ||
        height < margin * 2 + grid_rows * (min_room_height + room_gutter)) {
        std::fprintf(stderr, "Map::make_spec: Invalid configuration\n");
        return spec;
    }

    const std::size_t usable_width  = width - margin * 2;
    const std::size_t usable_height = height - margin * 2;
    const std::size_t cell_width    = usable_width / grid_columns;
    const std::size_t cell_height   = usable_height / grid_rows;

    // Every cell in a 3x3 grid optionally holds one room. This keeps rooms
    // apart and leaves open gutters for orthogonal corridors.
    std::array<bool, grid_columns * grid_rows> has_room{};
    std::size_t room_count = 0;

    for (bool& cell : has_room) {
        cell = _chance.roll(chance::ROOM_PRESENT);
        if (cell) {
            ++room_count;
        }
    }

    while (room_count < min_rooms) {
        const std::size_t index = _chance.range(0, has_room.size() - 1);

        if (!has_room[index]) {
            has_room[index] = true;
            ++room_count;
        }
    }

    spec.rooms.reserve(room_count);

    for (std::size_t row = 0; row < grid_rows; ++row) {
        for (std::size_t column = 0; column < grid_columns; ++column) {
            if (!has_room[row * grid_columns + column]) {
                continue;
            }

            const std::size_t cell_x = margin + column * cell_width;
            const std::size_t cell_y = margin + row * cell_height;

            const std::size_t max_width  = std::min(max_room_width, cell_width - room_gutter);
            const std::size_t max_height =
                std::min(max_room_height, cell_height >= min_room_height + 3
                                              ? cell_height - 3
                                              : cell_height - room_gutter);
            const std::size_t room_width =
                _chance.range(std::min(min_room_width, max_width), max_width);
            const std::size_t room_height =
                _chance.range(std::min(min_room_height, max_height), max_height);

            // Keep rooms roughly centred in their cell, but nudge them a little
            // so the dungeon does not look like an exact grid. The clamp keeps
            // at least one open cell on every side for corridors.
            constexpr std::size_t max_jitter = 2;

            const std::size_t spare_width  = cell_width - room_width;
            const std::size_t spare_height = cell_height - room_height;
            const std::size_t center_x     = spare_width / 2;
            const std::size_t center_y     = spare_height / 2;
            const std::size_t jitter_x =
                std::min(max_jitter, std::min(center_x - 1, spare_width - 1 - center_x));
            const std::size_t jitter_y =
                std::min(max_jitter, std::min(center_y - 1, spare_height - 1 - center_y));

            const std::size_t room_x =
                cell_x + _chance.range(center_x - jitter_x, center_x + jitter_x);
            const std::size_t room_y =
                cell_y + _chance.range(center_y - jitter_y, center_y + jitter_y);

            spec.rooms.push_back(RoomSpec{room_x, room_y, room_width, room_height});
        }
    }

    const auto side_facing = [](const RoomSpec& from, const RoomSpec& to) {
        const std::ptrdiff_t from_x = static_cast<std::ptrdiff_t>(from.x + from.width / 2);
        const std::ptrdiff_t from_y = static_cast<std::ptrdiff_t>(from.y + from.height / 2);
        const std::ptrdiff_t to_x   = static_cast<std::ptrdiff_t>(to.x + to.width / 2);
        const std::ptrdiff_t to_y   = static_cast<std::ptrdiff_t>(to.y + to.height / 2);

        const std::ptrdiff_t dx = to_x - from_x;
        const std::ptrdiff_t dy = to_y - from_y;

        if (std::abs(dx) >= std::abs(dy)) {
            return dx >= 0 ? DoorSide::Right : DoorSide::Left;
        }
        return dy >= 0 ? DoorSide::Bottom : DoorSide::Top;
    };

    const auto center_distance = [](const RoomSpec& a, const RoomSpec& b) {
        const std::ptrdiff_t ax = static_cast<std::ptrdiff_t>(a.x + a.width / 2);
        const std::ptrdiff_t ay = static_cast<std::ptrdiff_t>(a.y + a.height / 2);
        const std::ptrdiff_t bx = static_cast<std::ptrdiff_t>(b.x + b.width / 2);
        const std::ptrdiff_t by = static_cast<std::ptrdiff_t>(b.y + b.height / 2);

        return static_cast<std::size_t>(std::abs(bx - ax) + std::abs(by - ay));
    };

    for (std::size_t i = 1; i < spec.rooms.size(); ++i) {
        std::size_t nearest          = 0;
        std::size_t nearest_distance = std::numeric_limits<std::size_t>::max();

        for (std::size_t j = 0; j < i; ++j) {
            const std::size_t distance = center_distance(spec.rooms[i], spec.rooms[j]);
            if (distance < nearest_distance) {
                nearest_distance = distance;
                nearest          = j;
            }
        }

        const RoomSpec& from = spec.rooms[i];
        const RoomSpec& to   = spec.rooms[nearest];

        spec.connections.push_back(
            ConnectionSpec{i, nearest, side_facing(from, to), side_facing(to, from)});
    }

    const auto room_center = [](const RoomSpec& room) {
        return Point{room.x + room.width / 2, room.y + room.height / 2};
    };

    const std::size_t entrance_room = _chance.range(0, spec.rooms.size() - 1);
    std::size_t       exit_room     = entrance_room;
    std::size_t       exit_distance = 0;

    for (std::size_t i = 0; i < spec.rooms.size(); ++i) {
        if (i == entrance_room) {
            continue;
        }

        const std::size_t distance = center_distance(spec.rooms[entrance_room], spec.rooms[i]);
        if (distance >= exit_distance) {
            exit_distance = distance;
            exit_room     = i;
        }
    }

    spec.entrance = room_center(spec.rooms[entrance_room]);
    spec.exit     = room_center(spec.rooms[exit_room]);

    const auto adjacent_to_entrance = [&spec, entrance_room](std::size_t room) {
        for (const ConnectionSpec& connection : spec.connections) {
            if ((connection.from == entrance_room && connection.to == room) ||
                (connection.to == entrance_room && connection.from == room)) {
                return true;
            }
        }
        return false;
    };

    std::vector<double> key_weights(spec.rooms.size(), chance::KEY_DISTANT_ROOM_WEIGHT);
    for (std::size_t i = 0; i < spec.rooms.size(); ++i) {
        if (i == entrance_room) {
            key_weights[i] = chance::KEY_SAME_ROOM_WEIGHT;
        } else if (adjacent_to_entrance(i)) {
            key_weights[i] = chance::KEY_ADJACENT_ROOM_WEIGHT;
        }
    }

    const std::size_t key_room = _chance.weighted(key_weights);

    const auto roll_chest_contents = [this]() {
        std::vector<entities::ItemKind> contents;
        const std::size_t count =
            _chance.range(chance::CHEST_LOOT_MIN, chance::CHEST_LOOT_MAX);
        contents.reserve(count);

        for (std::size_t i = 0; i < count; ++i) {
            contents.push_back(_chance.roll(chance::CHEST_LOOT_DOUBLE_COIN)
                                   ? entities::ItemKind::CoinDouble
                                   : entities::ItemKind::Coin);
        }

        return contents;
    };

    const auto make_item_spec = [&roll_chest_contents](entities::ItemKind kind, Point cell) {
        ItemSpec item;
        item.kind = kind;
        item.x    = cell.x;
        item.y    = cell.y;

        if (kind == entities::ItemKind::Chest) {
            item.contents = roll_chest_contents();
        }

        return item;
    };

    for (std::size_t room_index = 0; room_index < spec.rooms.size(); ++room_index) {
        const RoomSpec&    room = spec.rooms[room_index];
        std::vector<Point> free_cells;

        for (std::size_t y = room.y + 1; y < room.y + room.height - 1; ++y) {
            for (std::size_t x = room.x + 1; x < room.x + room.width - 1; ++x) {
                if ((x == spec.entrance.x && y == spec.entrance.y) ||
                    (x == spec.exit.x && y == spec.exit.y)) {
                    continue;
                }
                free_cells.push_back(Point{x, y});
            }
        }

        if (room_index == key_room) {
            Point cell = room_center(room);

            if (!free_cells.empty()) {
                const std::size_t index = _chance.range(0, free_cells.size() - 1);
                cell                    = free_cells[index];
                free_cells[index]       = free_cells.back();
                free_cells.pop_back();
            }

            spec.items.push_back(make_item_spec(entities::ItemKind::Key, cell));
        }

        for (const chance::ItemSpawn& spawn : chance::ITEM_SPAWNS) {
            if (free_cells.empty()) {
                break;
            }
            if (!_chance.roll(spawn.probability)) {
                continue;
            }

            const std::size_t index = _chance.range(0, free_cells.size() - 1);
            const Point       cell  = free_cells[index];

            spec.items.push_back(make_item_spec(spawn.kind, cell));
            free_cells[index] = free_cells.back();
            free_cells.pop_back();
        }

        const double enemy_probability = room_index == entrance_room
                                             ? chance::ENEMY_ENTRANCE_ROOM_PRESENT
                                             : chance::ENEMY_ROOM_PRESENT;

        if (!free_cells.empty() && _chance.roll(enemy_probability)) {
            static constexpr double enemy_weights[] = {chance::FAST_ENEMY_WEIGHT,
                                                       chance::NORMAL_ENEMY_WEIGHT,
                                                       chance::BRUTE_ENEMY_WEIGHT};

            const std::size_t enemy_count =
                _chance.range(chance::ENEMY_MIN_PER_ROOM, chance::ENEMY_MAX_PER_ROOM);

            for (std::size_t i = 0; i < enemy_count && !free_cells.empty(); ++i) {
                const std::size_t index = _chance.range(0, free_cells.size() - 1);
                const Point       cell  = free_cells[index];
                const auto kind =
                    static_cast<entities::EnemyKind>(_chance.weighted(enemy_weights));

                spec.enemies.push_back(EnemySpec{kind, cell.x, cell.y});

                free_cells[index] = free_cells.back();
                free_cells.pop_back();
            }
        }
    }

    return spec;
}

void Map::generate(const MapSpec& spec) {
    clear();

    if (spec.width == 0 || spec.height == 0) {
        return;
    }

    _width  = spec.width;
    _height = spec.height;
    _tiles.assign(_width * _height, Tile::Empty);

    for (const ConnectionSpec& connection : spec.connections) {
        if (connection.from >= spec.rooms.size() || connection.to >= spec.rooms.size()) {
            throw std::out_of_range(
                "Map::generate: connection references a room that does not exist");
        }
    }

    for (const RoomSpec& spec_room : spec.rooms) {
        room(spec_room.x, spec_room.y, spec_room.width, spec_room.height);
    }

    for (const ConnectionSpec& connection : spec.connections) {
        const RoomSpec& from = spec.rooms[connection.from];
        const RoomSpec& to   = spec.rooms[connection.to];

        const Door from_door = door(from.x, from.y, from.width, from.height, connection.from_side);
        const Door to_door   = door(to.x, to.y, to.width, to.height, connection.to_side);

        walkway(from_door.x, from_door.y, to_door.x, to_door.y, spec.rooms);
    }

    const auto is_floor = [this](const Point& point) {
        return point.x < _width && point.y < _height && at(point.x, point.y) == Tile::Floor;
    };

    _entrance = {};
    _exit     = {};
    _has_exit = false;

    if (is_floor(spec.entrance)) {
        _entrance = spec.entrance;
        add(std::make_shared<entities::Player>(spec.entrance.x, spec.entrance.y));
    }

    if (is_floor(spec.exit)) {
        _exit     = spec.exit;
        _has_exit = true;
    }

    for (const ItemSpec& item : spec.items) {
        if (item.x < _width && item.y < _height && at(item.x, item.y) == Tile::Floor) {
            add(make_item_entity(item));
        }
    }

    for (const EnemySpec& enemy : spec.enemies) {
        if (enemy.x < _width && enemy.y < _height && at(enemy.x, enemy.y) == Tile::Floor) {
            add(make_enemy_entity(enemy));
        }
    }
}

void Map::generate(std::size_t width, std::size_t height) {
    generate(make_spec(width, height));
}

} // namespace dungeon::map
