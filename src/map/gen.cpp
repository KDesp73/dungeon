#include "dungeon/map/map.hpp"

#include <cstddef>
#include <cstdlib>
#include <stdexcept>

namespace dungeon::map {

Map::Door Map::room(std::size_t x, std::size_t y, std::size_t width, std::size_t height,
                     DoorSide door_side)
{
    if (width < 3 || height < 3) {
        throw std::invalid_argument("Map::room: room must be at least 3x3");
    }

    if (_width == 0 || _height == 0 || x >= _width || y >= _height || width > _width - x ||
        height > _height - y) {
        throw std::out_of_range("Map::room: room does not fit inside the map");
    }

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

    std::size_t door_x = x + width / 2;
    std::size_t door_y = y + height / 2;

    switch (door_side) {
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

    constexpr std::size_t room_width  = 16;
    constexpr std::size_t room_height = 10;
    constexpr std::size_t margin      = 2;
    constexpr std::size_t min_gap     = 4;

    if (width < room_width * 2 + margin * 2 + min_gap || height < room_height + margin * 2) {
        return;
    }

    const std::size_t room_y  = (height - room_height) / 2;
    const std::size_t left_x  = margin;
    const std::size_t right_x = width - room_width - margin;

    const Door left_door =
        room(left_x, room_y, room_width, room_height, DoorSide::Right);
    const Door right_door =
        room(right_x, room_y, room_width, room_height, DoorSide::Left);

    walkway(left_door.x, left_door.y, right_door.x, right_door.y);
}

} // namespace dungeon::map

