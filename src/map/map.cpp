#include "dungeon/map/map.hpp"

#include <cstdio>
#include <stdexcept>

namespace dungeon::map {

char glyph(Tile tile) noexcept
{
    switch (tile) {
    case Tile::Wall:
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

void Map::generate(std::size_t width, std::size_t height)
{
    clear();

    if (width == 0 || height == 0) {
        return;
    }

    _width  = width;
    _height = height;
    _tiles.assign(width * height, Tile::Floor);

    for (std::size_t x = 0; x < _width; ++x) {
        set(x, 0, Tile::Wall);
        set(x, _height - 1, Tile::Wall);
    }

    for (std::size_t y = 0; y < _height; ++y) {
        set(0, y, Tile::Wall);
        set(_width - 1, y, Tile::Wall);
    }
}

void Map::clear()
{
    _width  = 0;
    _height = 0;
    _tiles.clear();
}

Tile Map::at(std::size_t x, std::size_t y) const
{
    if (x >= _width || y >= _height) {
        throw std::out_of_range("Map::at: coordinates are outside the map");
    }
    return _tiles.at(y * _width + x);
}

void Map::set(std::size_t x, std::size_t y, Tile tile)
{
    if (x >= _width || y >= _height) {
        throw std::out_of_range("Map::set: coordinates are outside the map");
    }
    _tiles[y * _width + x] = tile;
}

void Map::print(FILE* fd) const
{
    if (fd == nullptr) {
        return;
    }

    for (std::size_t y = 0; y < _height; ++y) {
        for (std::size_t x = 0; x < _width; ++x) {
            std::fputc(glyph(at(x, y)), fd);
            // std::fputc(glyph(at(x, y)), fd); // NOTE: displaying double for width to match dimensions
        }
        std::fputc('\n', fd);
    }
}

} // namespace dungeon::map
