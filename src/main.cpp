#include "dungeon/core/term.hpp"
#include "dungeon/core/version.hpp"
#include "dungeon/map/map.hpp"

#include <iostream>

int main() {
    std::cout << dungeon::core::kProjectName << ' ' << dungeon::core::kVersion << '\n';

    auto map = dungeon::map::Map();

    int width = 0, height = 0;
    dungeon::core::term_size(width, height);

    map.generate(static_cast<std::size_t>(width/2), static_cast<std::size_t>(height/2));
    map.print();

    return 0;
}
