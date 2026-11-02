#include "dungeon/controls.hpp"
#include "dungeon/core/ticker.hpp"
#include "dungeon/core/version.hpp"
#include "dungeon/display/buffer.hpp"
#include "dungeon/display/term.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/game.hpp"

#include <CLI/CLI.hpp>

#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

namespace {

volatile std::sig_atomic_t g_running = 1;

void handle_interrupt(int) {
    g_running = 0;
}

} // namespace

int main(int argc, char** argv) {
    CLI::App app{"A real-time terminal action RPG", std::string(dungeon::core::kProjectName)};

    const std::string version =
        std::string(dungeon::core::kProjectName) + ' ' + std::string(dungeon::core::kVersion);
    app.set_version_flag("-v,--version", version);

    std::uint32_t      seed        = 0;
    const CLI::Option* seed_option = app.add_option(
        "-s,--seed", seed, "Seed used to generate the dungeon");
    bool print_map = false;
    app.add_flag("-p,--print-map", print_map, "Print a generated dungeon map and exit");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        return app.exit(error);
    }

    const bool has_seed = seed_option->count() > 0;

    if (print_map) {
        dungeon::map::Map map;
        if (has_seed) {
            map.reseed(seed);
        }
        map.generate();

        std::cerr << "Seed: " << map.seed() << '\n';
        map.print(stdout);
        return 0;
    }

    std::signal(SIGINT, handle_interrupt);

    auto game = dungeon::game::Game();
    if (has_seed) {
        game.map().reseed(seed);
    }

    auto       buffer = dungeon::display::FrameBuffer();
    const auto layout = dungeon::display::tui::make_layout(buffer.width(), buffer.height());

    const std::size_t map_width  = layout.game.width > 2 ? layout.game.width - 2 : 0;
    const std::size_t map_height = layout.game.height > 2 ? layout.game.height - 2 : 0;

    game.generate(map_width, map_height);

    std::cout << dungeon::core::kProjectName << ' ' << dungeon::core::kVersion << '\n'
              << "Seed: " << game.map().seed() << '\n';

    const auto tick_interval = std::chrono::duration_cast<dungeon::core::Ticker::Duration>(
        std::chrono::duration<double>(1.0 / 60.0));
    dungeon::core::Ticker ticker(tick_interval);

    const dungeon::controls::TerminalSession terminal;
    dungeon::display::tui::hide_cursor();
    while (g_running) {
        const std::size_t updates = ticker.advance();

        for (std::size_t i = 0; i < updates; ++i) {
            // update_game(ticker.interval());
        }

        const dungeon::controls::KeyEvent key = dungeon::controls::poll_key();
        if (key.kind == dungeon::controls::KeyKind::Escape) {
            break;
        }

        if (const auto direction = dungeon::game::direction_from_key(key)) {
            static_cast<void>(game.move(*direction));
        } else if (key.kind == dungeon::controls::KeyKind::Character) {
            switch (key.value) {
            case '[':
                game.cycle_chest_selection(-1);
                break;
            case ']':
                game.cycle_chest_selection(1);
                break;
            case '\r':
            case '\n':
                static_cast<void>(game.take_selected_chest_item());
                break;
            default:
                if (key.value >= '1' && key.value <= '9') {
                    game.select_chest_item(static_cast<std::size_t>(key.value - '1'));
                }
                break;
            }
        }

        const dungeon::display::tui::HudState hud{
            game.messages(), game.chest_selection()};
        dungeon::display::tui::draw_ui(buffer, game.map(), layout, hud);
        buffer.present();

        if (g_running) {
            std::this_thread::sleep_until(ticker.next_tick());
        }
    }
    dungeon::display::tui::show_cursor();

    return 0;
}
