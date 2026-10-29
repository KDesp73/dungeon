#include "dungeon/map/map.hpp"

#include <stdexcept>

namespace dungeon::map {

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
        }
        std::fputc('\n', fd);
    }
}


} // namespace dungeon::map
