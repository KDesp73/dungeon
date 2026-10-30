#include "dungeon/core/ticker.hpp"
#include "dungeon/core/version.hpp"
#include "dungeon/display/buffer.hpp"
#include "dungeon/display/term.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/map/map.hpp"

#include <chrono>
#include <csignal>
#include <cstddef>
#include <iostream>
#include <thread>

namespace {

volatile std::sig_atomic_t g_running = 1;

void handle_interrupt(int) { g_running = 0; }

} // namespace

int main() {
    std::signal(SIGINT, handle_interrupt);

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

    const auto tick_interval =
        std::chrono::duration_cast<dungeon::core::Ticker::Duration>(
            std::chrono::duration<double>(1.0 / 60.0));
    dungeon::core::Ticker ticker(tick_interval);

    dungeon::display::tui::hide_cursor();
    while (g_running) {
        const std::size_t updates = ticker.advance();

        for (std::size_t i = 0; i < updates; ++i) {
            // update_game(ticker.interval());
        }

        dungeon::display::tui::draw_ui(buffer, map, layout);
        buffer.present();

        if (g_running) {
            std::this_thread::sleep_until(ticker.next_tick());
        }
    }
    dungeon::display::tui::show_cursor();

    return 0;
}
