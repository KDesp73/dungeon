#include "dungeon/display/buffer.hpp"
#include "dungeon/display/term.hpp"
#include "dungeon/core/version.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/map/map.hpp"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <thread>
#include <unistd.h>

int main() {
    std::cout << dungeon::core::kProjectName << ' ' << dungeon::core::kVersion << '\n';

    auto map = dungeon::map::Map();
    std::cout << "Seed: " << map.seed() << '\n';

    int width = 0, height = 0;
    dungeon::display::tui::term_size(width, height);

    map.generate(static_cast<std::size_t>(width/2), static_cast<std::size_t>(height/2));
    // map.print();

    auto buffer = dungeon::display::FrameBuffer();
    bool running = true;

    while (running) {
        // 1. Handle Input (e.g., check keyboard for movement or quit)
        // running = handle_input();

        // 2. Update Game State (move entities, check collisions)
        // update_game();

        dungeon::display::tui::label(buffer, "Hello World", 10, 10);

        // 4. Swap buffers and render to the terminal
        buffer.present();

        // 5. Frame pacing (~60 FPS -> 16ms per frame)
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}
