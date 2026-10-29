#include "dungeon/map/map.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <limits>
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

void Map::walkway(std::size_t start_x, std::size_t start_y, std::size_t end_x, std::size_t end_y)
{
    if (_width == 0 || _height == 0 || start_x >= _width || start_y >= _height ||
        end_x >= _width || end_y >= _height) {
        throw std::out_of_range("Map::walkway: endpoint is outside the map");
    }

    std::ptrdiff_t       x  = static_cast<std::ptrdiff_t>(start_x);
    std::ptrdiff_t       y  = static_cast<std::ptrdiff_t>(start_y);
    const std::ptrdiff_t x1 = static_cast<std::ptrdiff_t>(end_x);
    const std::ptrdiff_t y1 = static_cast<std::ptrdiff_t>(end_y);

    const std::ptrdiff_t dx     = std::abs(x1 - x);
    const std::ptrdiff_t dy     = -std::abs(y1 - y);
    const std::ptrdiff_t step_x = x < x1 ? 1 : -1;
    const std::ptrdiff_t step_y = y < y1 ? 1 : -1;

    std::ptrdiff_t error = dx + dy;

    while (true) {
        const std::size_t cell_x = static_cast<std::size_t>(x);
        const std::size_t cell_y = static_cast<std::size_t>(y);

        if (at(cell_x, cell_y) != Tile::Door) {
            set(cell_x, cell_y, Tile::Walkway);
        }

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
}

void Map::generate(std::size_t width, std::size_t height)
{
    clear();

    if (width == 0 || height == 0) {
        return;
    }

    _width  = width;
    _height = height;
    _tiles.assign(width * height, Tile::Floor);

    constexpr std::size_t margin          = 2;
    constexpr std::size_t min_room_width  = 6;
    constexpr std::size_t max_room_width  = 14;
    constexpr std::size_t min_room_height = 4;
    constexpr std::size_t max_room_height = 8;
    constexpr std::size_t room_padding    = 1;
    constexpr std::size_t min_rooms       = 2;
    constexpr std::size_t max_rooms       = 8;
    constexpr std::size_t placement_tries = 50;

    if (width < margin * 2 + min_room_width || height < margin * 2 + min_room_height) {
        return;
    }

    const std::size_t usable_width  = width - margin * 2;
    const std::size_t usable_height = height - margin * 2;
    const std::size_t area          = usable_width * usable_height;

    const std::size_t capacity =
        std::max(min_rooms, area / (max_room_width * max_room_height));
    const std::size_t room_limit = std::min(max_rooms, capacity);

    std::uniform_int_distribution<std::size_t> room_count_dist(min_rooms, room_limit);
    const std::size_t target_rooms = room_count_dist(_rng);

    struct RoomSpec {
        std::size_t x;
        std::size_t y;
        std::size_t width;
        std::size_t height;
    };

    std::vector<RoomSpec> rooms;
    rooms.reserve(target_rooms);

    std::uniform_int_distribution<std::size_t> width_dist(
        min_room_width, std::min(max_room_width, usable_width));
    std::uniform_int_distribution<std::size_t> height_dist(
        min_room_height, std::min(max_room_height, usable_height));

    const std::size_t max_attempts = target_rooms * placement_tries;

    for (std::size_t attempt = 0; attempt < max_attempts && rooms.size() < target_rooms;
         ++attempt) {
        const std::size_t room_width  = width_dist(_rng);
        const std::size_t room_height = height_dist(_rng);

        std::uniform_int_distribution<std::size_t> x_dist(margin, width - margin - room_width);
        std::uniform_int_distribution<std::size_t> y_dist(margin, height - margin - room_height);

        const std::size_t room_x = x_dist(_rng);
        const std::size_t room_y = y_dist(_rng);

        bool overlaps = false;
        for (const RoomSpec& other : rooms) {
            if (room_x < other.x + other.width + room_padding &&
                room_x + room_width + room_padding > other.x &&
                room_y < other.y + other.height + room_padding &&
                room_y + room_height + room_padding > other.y) {
                overlaps = true;
                break;
            }
        }

        if (!overlaps) {
            rooms.push_back(RoomSpec{room_x, room_y, room_width, room_height});
        }
    }

    if (rooms.size() < min_rooms) {
        return;
    }

    for (const RoomSpec& spec : rooms) {
        room(spec.x, spec.y, spec.width, spec.height);
    }

    const auto side_facing = [](const RoomSpec& from, const RoomSpec& to) {
        const std::ptrdiff_t from_x =
            static_cast<std::ptrdiff_t>(from.x + from.width / 2);
        const std::ptrdiff_t from_y =
            static_cast<std::ptrdiff_t>(from.y + from.height / 2);
        const std::ptrdiff_t to_x = static_cast<std::ptrdiff_t>(to.x + to.width / 2);
        const std::ptrdiff_t to_y = static_cast<std::ptrdiff_t>(to.y + to.height / 2);

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

    for (std::size_t i = 1; i < rooms.size(); ++i) {
        std::size_t nearest          = 0;
        std::size_t nearest_distance = std::numeric_limits<std::size_t>::max();

        for (std::size_t j = 0; j < i; ++j) {
            const std::size_t distance = center_distance(rooms[i], rooms[j]);
            if (distance < nearest_distance) {
                nearest_distance = distance;
                nearest          = j;
            }
        }

        const RoomSpec& from = rooms[i];
        const RoomSpec& to   = rooms[nearest];

        const Door from_door =
            door(from.x, from.y, from.width, from.height, side_facing(from, to));
        const Door to_door = door(to.x, to.y, to.width, to.height, side_facing(to, from));

        walkway(from_door.x, from_door.y, to_door.x, to_door.y);
    }
}

} // namespace dungeon::map

