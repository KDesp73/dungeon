#include "dungeon/core/version.hpp"
#include "dungeon/map/map.hpp"

#include <iostream>

int main() {
    std::cout << dungeon::core::kProjectName << ' ' << dungeon::core::kVersion << '\n';

    auto map = dungeon::map::Map();

    map.generate(32, 32);

    map.set(10, 10, dungeon::map::Tile::Player);

    map.print();

    return 0;
}
