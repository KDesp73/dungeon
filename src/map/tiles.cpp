#include "dungeon/map/map.hpp"

#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace dungeon::map {

char glyph(Tile tile) noexcept
{
    switch (tile) {
    case Tile::Empty:
        return ' ';
    case Tile::HWall:
        return '-';
    case Tile::VWall:
        return '|';
    case Tile::Walkway:
        return '#';
    case Tile::Floor:
        return '.';
    case Tile::Player:
        return '@';
    case Tile::Door:
        return 'D';
    case Tile::Key:
        return 'K';
    }
    return '?';
}

bool Map::is_wall(std::size_t index)
{
    if (index >= _tiles.size()) {
        throw std::out_of_range("Map::is_wall: index is outside the map");
    }

    const Tile tile = _tiles.at(index);
    return tile == Tile::HWall || tile == Tile::VWall;
}

bool Map::is_wall(std::size_t x, std::size_t y)
{
    const Tile tile = at(x, y);
    return tile == Tile::HWall || tile == Tile::VWall;
}


} // namespace dungeon::map
