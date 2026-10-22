#include "dungeon/core/version.hpp"

#include <iostream>

int main() {
    std::cout << dungeon::core::kProjectName << ' ' << dungeon::core::kVersion << '\n';
    return 0;
}
