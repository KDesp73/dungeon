#include "dungeon/map/map.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dungeon::map {

void Map::clear()
{
    _width    = 0;
    _height   = 0;
    _entrance = {};
    _exit     = {};
    _tiles.clear();
    _entities.clear();
    _player = nullptr;
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

bool Map::walkable(std::size_t x, std::size_t y) const noexcept
{
    if (x >= _width || y >= _height) {
        return false;
    }

    switch (_tiles[y * _width + x]) {
    case Tile::Floor:
    case Tile::Walkway:
    case Tile::Door:
        return true;
    case Tile::Empty:
    case Tile::HWall:
    case Tile::VWall:
        return false;
    }

    return false;
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



int Map::add(EntityPtr entity)
{
    if (!entity) {
        return 0;
    }

    const int id = entity->id();

    if (auto* player = dynamic_cast<entities::Player*>(entity.get()); player != nullptr) {
        _player = player;
    }

    _entities.push_back(std::move(entity));
    return id;
}

bool Map::remove(int id)
{
    const auto entity = std::find_if(_entities.begin(), _entities.end(),
                                     [id](const EntityPtr& candidate) {
                                         return candidate && candidate->id() == id;
                                     });

    if (entity == _entities.end()) {
        return false;
    }

    if (entity->get() == _player) {
        _player = nullptr;
    }

    _entities.erase(entity);
    return true;
}

std::vector<entities::Entity> Map::entities() const
{
    std::vector<entities::Entity> copies;
    copies.reserve(_entities.size());

    for (const EntityPtr& entity : _entities) {
        if (entity) {
            copies.push_back(*entity);
        }
    }

    return copies;
}

} // namespace dungeon::map
