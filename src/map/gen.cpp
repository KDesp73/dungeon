#include "dungeon/map/map.hpp"

#include <cstddef>
#include <cstdlib>
#include <stdexcept>

namespace dungeon::map {

void Map::room(std::size_t x, std::size_t y, std::size_t width, std::size_t height)
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
        set(static_cast<std::size_t>(x), static_cast<std::size_t>(y), Tile::Walkway);

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

    constexpr std::size_t walkwayStartX = 4;
    constexpr std::size_t walkwayStartY = 29;
    constexpr std::size_t walkwayEndX   = 25;
    constexpr std::size_t walkwayEndY   = 20;

    if (width > walkwayEndX && height > walkwayStartY) {
        walkway(walkwayStartX, walkwayStartY, walkwayEndX, walkwayEndY);
    }
}

} // namespace dungeon::map

