#include "dungeon/core/version.hpp"
#include "dungeon/display/buffer.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/map/map.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>

int main() {
    std::cout << dungeon::core::kProjectName << ' ' << dungeon::core::kVersion << '\n';

    auto map = dungeon::map::Map();
    std::cout << "Seed: " << map.seed() << '\n';

    auto buffer = dungeon::display::FrameBuffer();
    const auto layout =
        dungeon::display::tui::make_layout(buffer.width(), buffer.height());

    const std::size_t map_width =
        layout.game.width > 2 ? layout.game.width - 2 : 0;
    const std::size_t map_height =
        layout.game.height > 2 ? layout.game.height - 2 : 0;

    map.generate(map_width, map_height);

    while (true) {
        dungeon::display::tui::draw_ui(buffer, map, layout);
        buffer.present();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}
