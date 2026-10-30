#include "dungeon/map/entities.hpp"

namespace dungeon::map::entities {

void Entity::place(std::size_t xx, std::size_t yy)
{
    this->x = xx;
    this->y = yy;
}

} // namespace dungeon::map::entities

