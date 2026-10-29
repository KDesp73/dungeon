#include "dungeon/core/version.hpp"
#include "dungeon/map/map.hpp"

#include <iostream>

int main() {
    std::cout << dungeon::core::kProjectName << ' ' << dungeon::core::kVersion << '\n';

    auto map = dungeon::map::Map();

    map.generate();
    map.print();

    return 0;
}
