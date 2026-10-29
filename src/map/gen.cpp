#include "dungeon/map/map.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>

namespace dungeon::map {

void Map::validate_room(std::size_t x, std::size_t y, std::size_t width,
                        std::size_t height) const
{
    if (width < 3 || height < 3) {
        throw std::invalid_argument("Map::room: room must be at least 3x3");
    }

    if (_width == 0 || _height == 0 || x >= _width || y >= _height || width > _width - x ||
        height > _height - y) {
        throw std::out_of_range("Map::room: room does not fit inside the map");
    }
}

void Map::room(std::size_t x, std::size_t y, std::size_t width, std::size_t height)
{
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
                    DoorSide side)
{
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
                  const std::vector<RoomSpec>& rooms)
{
    if (_width == 0 || _height == 0 || start_x >= _width || start_y >= _height ||
        end_x >= _width || end_y >= _height) {
        throw std::out_of_range("Map::walkway: endpoint is outside the map");
    }

    const auto inside_room = [&rooms](std::size_t x, std::size_t y) {
        for (const RoomSpec& room : rooms) {
            if (x >= room.x && x < room.x + room.width && y >= room.y &&
                y < room.y + room.height) {
                return true;
            }
        }
        return false;
    };

    const std::size_t start = start_y * _width + start_x;
    const std::size_t goal  = end_y * _width + end_x;

    // Prefer the original Bresenham line, including its diagonal steps. Only
    // fall back to pathfinding when the line would cut through a room.
    {
        std::ptrdiff_t x = static_cast<std::ptrdiff_t>(start_x);
        std::ptrdiff_t y = static_cast<std::ptrdiff_t>(start_y);

        const std::ptrdiff_t x1     = static_cast<std::ptrdiff_t>(end_x);
        const std::ptrdiff_t y1     = static_cast<std::ptrdiff_t>(end_y);
        const std::ptrdiff_t dx     = std::abs(x1 - x);
        const std::ptrdiff_t dy     = -std::abs(y1 - y);
        const std::ptrdiff_t step_x = x < x1 ? 1 : -1;
        const std::ptrdiff_t step_y = y < y1 ? 1 : -1;

        std::ptrdiff_t error = dx + dy;
        bool crosses_room     = false;

        std::vector<std::size_t> straight;
        straight.reserve(static_cast<std::size_t>(std::max(dx, -dy)) + 1);

        while (true) {
            const std::size_t cell_x = static_cast<std::size_t>(x);
            const std::size_t cell_y = static_cast<std::size_t>(y);
            const std::size_t index  = cell_y * _width + cell_x;

            if (index != start && index != goal && inside_room(cell_x, cell_y)) {
                crosses_room = true;
                break;
            }

            straight.push_back(index);

            if (x == x1 && y == y1) {
                break;
            }

            const std::ptrdiff_t doubled_error = 2 * error;
            if (doubled_error >= dy) {
                error += dy;
                x += step_x;
            }
            if (doubled_error <= dx) {
                error += dx;
                y += step_y;
            }
        }

        if (!crosses_room) {
            for (const std::size_t index : straight) {
                const std::size_t cell_x = index % _width;
                const std::size_t cell_y = index / _width;

                if (at(cell_x, cell_y) != Tile::Door) {
                    set(cell_x, cell_y, Tile::Walkway);
                }
            }
            return;
        }
    }

    constexpr std::size_t margin = 2;

    if (_width <= margin * 2 || _height <= margin * 2) {
        throw std::out_of_range("Map::walkway: map is too small for a walkway");
    }

    const std::size_t min_x = margin;
    const std::size_t max_x = _width - margin - 1;
    const std::size_t min_y = margin;
    const std::size_t max_y = _height - margin - 1;

    const auto is_blocked = [&](std::size_t index) {
        const std::size_t x = index % _width;
        const std::size_t y = index / _width;

        if (index == start || index == goal) {
            return false;
        }

        if (x < min_x || x > max_x || y < min_y || y > max_y) {
            return true;
        }

        return inside_room(x, y) || is_wall(index);
    };

    constexpr std::size_t direction_count = 9; // 0 = start, 1..8 = directions
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
        bool operator()(const QueueEntry& lhs, const QueueEntry& rhs) const
        {
            return lhs.f > rhs.f;
        }
    };

    std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueEntryGreater> open;

    const auto heuristic = [&](std::size_t index) {
        const std::size_t x  = index % _width;
        const std::size_t y  = index / _width;
        const std::size_t dx = x > end_x ? x - end_x : end_x - x;
        const std::size_t dy = y > end_y ? y - end_y : end_y - y;
        return std::max(dx, dy);
    };

    const std::size_t start_state = start * direction_count;
    best[start_state]             = 0;
    open.push(QueueEntry{heuristic(start), start_state});

    std::size_t goal_state = infinity;

    constexpr int offset_x[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    constexpr int offset_y[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

    while (!open.empty()) {
        const QueueEntry entry = open.top();
        open.pop();

        const std::size_t state = entry.state;
        const std::size_t index = state / direction_count;
        const int direction     = static_cast<int>(state % direction_count);

        if (entry.f > best[state] + heuristic(index)) {
            continue;
        }

        if (index == goal) {
            goal_state = state;
            break;
        }

        const std::size_t x = index % _width;
        const std::size_t y = index / _width;

        for (int move = 0; move < 8; ++move) {
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

            const bool diagonal = offset_x[move] != 0 && offset_y[move] != 0;
            if (diagonal) {
                const std::size_t side_x =
                    static_cast<std::size_t>(next_x) + (_width * y);
                const std::size_t side_y =
                    static_cast<std::size_t>(next_y) * _width + x;

                if (is_blocked(side_x) || is_blocked(side_y)) {
                    continue;
                }
            }

            const int next_direction = move + 1;
            std::size_t cost         = best[state] + 1;

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

void Map::fill_walkway_corners(const std::vector<RoomSpec>& rooms)
{
    const auto inside_room = [&rooms](std::size_t x, std::size_t y) {
        for (const RoomSpec& room : rooms) {
            if (x >= room.x && x < room.x + room.width && y >= room.y &&
                y < room.y + room.height) {
                return true;
            }
        }
        return false;
    };

    const auto is_path = [this](std::size_t x, std::size_t y) {
        const Tile tile = at(x, y);
        return tile == Tile::Walkway || tile == Tile::Door;
    };

    const auto can_fill = [&](std::size_t x, std::size_t y) {
        return at(x, y) == Tile::Floor && !inside_room(x, y);
    };

    bool changed = true;

    while (changed) {
        changed = false;

        for (std::size_t y = 0; y < _height; ++y) {
            for (std::size_t x = 0; x < _width; ++x) {
                if (!is_path(x, y)) {
                    continue;
                }

                for (int offset_y = -1; offset_y <= 1; offset_y += 2) {
                    for (int offset_x = -1; offset_x <= 1; offset_x += 2) {
                        const int diagonal_x = static_cast<int>(x) + offset_x;
                        const int diagonal_y = static_cast<int>(y) + offset_y;

                        if (diagonal_x < 0 || diagonal_y < 0 ||
                            diagonal_x >= static_cast<int>(_width) ||
                            diagonal_y >= static_cast<int>(_height)) {
                            continue;
                        }

                        const std::size_t next_x = static_cast<std::size_t>(diagonal_x);
                        const std::size_t next_y = static_cast<std::size_t>(diagonal_y);

                        if (!is_path(next_x, next_y)) {
                            continue;
                        }

                        const bool already_connected =
                            is_path(next_x, y) || is_path(x, next_y);

                        if (already_connected) {
                            continue;
                        }

                        if (can_fill(next_x, y)) {
                            set(next_x, y, Tile::Walkway);
                            changed = true;
                        } else if (can_fill(x, next_y)) {
                            set(x, next_y, Tile::Walkway);
                            changed = true;
                        }
                    }
                }
            }
        }
    }
}

MapSpec Map::make_spec(std::size_t width, std::size_t height)
{
    MapSpec spec;
    spec.width  = width;
    spec.height = height;

    if (width == 0 || height == 0) {
        return spec;
    }

    constexpr std::size_t margin          = 2;
    constexpr std::size_t min_room_width  = 6;
    constexpr std::size_t max_room_width  = 14;
    constexpr std::size_t min_room_height = 4;
    constexpr std::size_t max_room_height = 8;
    constexpr std::size_t room_padding    = 3;
    constexpr std::size_t min_rooms       = 2;
    constexpr std::size_t max_rooms       = 8;
    constexpr std::size_t placement_tries = 50;

    if (width < margin * 2 + min_room_width || height < margin * 2 + min_room_height) {
        return spec;
    }

    const std::size_t usable_width  = width - margin * 2;
    const std::size_t usable_height = height - margin * 2;
    const std::size_t area          = usable_width * usable_height;

    const std::size_t capacity =
        std::max(min_rooms, area / (max_room_width * max_room_height));
    const std::size_t room_limit = std::min(max_rooms, capacity);

    std::uniform_int_distribution<std::size_t> room_count_dist(min_rooms, room_limit);
    const std::size_t target_rooms = room_count_dist(_rng);

    spec.rooms.reserve(target_rooms);

    std::uniform_int_distribution<std::size_t> width_dist(
        min_room_width, std::min(max_room_width, usable_width));
    std::uniform_int_distribution<std::size_t> height_dist(
        min_room_height, std::min(max_room_height, usable_height));

    const std::size_t max_attempts = target_rooms * placement_tries;

    for (std::size_t attempt = 0; attempt < max_attempts && spec.rooms.size() < target_rooms;
         ++attempt) {
        const std::size_t room_width  = width_dist(_rng);
        const std::size_t room_height = height_dist(_rng);

        std::uniform_int_distribution<std::size_t> x_dist(margin, width - margin - room_width);
        std::uniform_int_distribution<std::size_t> y_dist(margin, height - margin - room_height);

        const std::size_t room_x = x_dist(_rng);
        const std::size_t room_y = y_dist(_rng);

        bool overlaps = false;
        for (const RoomSpec& other : spec.rooms) {
            if (room_x < other.x + other.width + room_padding &&
                room_x + room_width + room_padding > other.x &&
                room_y < other.y + other.height + room_padding &&
                room_y + room_height + room_padding > other.y) {
                overlaps = true;
                break;
            }
        }

        if (!overlaps) {
            spec.rooms.push_back(RoomSpec{room_x, room_y, room_width, room_height});
        }
    }

    if (spec.rooms.size() < min_rooms) {
        spec.rooms.clear();
        return spec;
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

    return spec;
}

void Map::generate(const MapSpec& spec)
{
    clear();

    if (spec.width == 0 || spec.height == 0) {
        return;
    }

    _width  = spec.width;
    _height = spec.height;
    _tiles.assign(_width * _height, Tile::Floor);

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

        const Door from_door =
            door(from.x, from.y, from.width, from.height, connection.from_side);
        const Door to_door = door(to.x, to.y, to.width, to.height, connection.to_side);

        walkway(from_door.x, from_door.y, to_door.x, to_door.y, spec.rooms);
    }

    fill_walkway_corners(spec.rooms);
}

void Map::generate(std::size_t width, std::size_t height)
{
    generate(make_spec(width, height));
}

} // namespace dungeon::map

