#include "dungeon/controls.hpp"
#include "dungeon/core/ticker.hpp"
#include "dungeon/core/version.hpp"
#include "dungeon/display/buffer.hpp"
#include "dungeon/display/term.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/scene/game_scene.hpp"
#include "dungeon/scene/scene_manager.hpp"
#include "dungeon/scene/title_scene.hpp"

#include <CLI/CLI.hpp>

#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

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

    std::uint32_t      seed = 0;
    const CLI::Option* seed_option =
        app.add_option("-s,--seed", seed, "Seed used to generate the dungeon");
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

    auto       buffer = dungeon::display::FrameBuffer();
    const auto layout = dungeon::display::tui::make_layout(buffer.width(), buffer.height());

    const std::size_t map_width  = layout.game.width > 2 ? layout.game.width - 2 : 0;
    const std::size_t map_height = layout.game.height > 2 ? layout.game.height - 2 : 0;

    std::optional<std::uint32_t> seed_value;
    if (has_seed) {
        seed_value = seed;
    }

    dungeon::scene::SceneManager scenes;
    dungeon::scene::SceneContext context{scenes};

    dungeon::scene::TitleSceneConfig title;
    title.title = "DUNGEON";
    title.subtitle =
        std::string(dungeon::core::kProjectName) + ' ' + std::string(dungeon::core::kVersion);
    title.prompt   = "Press ENTER to start";
    title.hint     = "Esc to quit";
    title.on_start = [map_width, map_height,
                      seed_value](dungeon::scene::SceneContext& scene_context) {
        dungeon::scene::GameSceneConfig config;
        config.width  = map_width;
        config.height = map_height;
        config.seed   = seed_value;

        scene_context.scenes.replace(
            std::make_unique<dungeon::scene::GameScene>(std::move(config)));
    };
    scenes.push(std::make_unique<dungeon::scene::TitleScene>(std::move(title)));

    const auto tick_interval = std::chrono::duration_cast<dungeon::core::Ticker::Duration>(
        std::chrono::duration<double>(1.0 / 60.0));
    dungeon::core::Ticker ticker(tick_interval);

    const dungeon::controls::TerminalSession terminal;
    dungeon::display::tui::hide_cursor();
    while (g_running && !context.quit_requested) {
        const std::size_t updates = ticker.advance();

        for (std::size_t i = 0; i < updates; ++i) {
            scenes.update(ticker.interval(), context);
        }

        const dungeon::controls::KeyEvent key = dungeon::controls::poll_key();
        if (!key.is_none()) {
            static_cast<void>(scenes.handle_key(key, context));
        }

        if (!g_running || context.quit_requested) {
            break;
        }

        scenes.draw(buffer, context);
        buffer.present();

        if (g_running) {
            std::this_thread::sleep_until(ticker.next_tick());
        }
    }
    dungeon::display::tui::show_cursor();

    return 0;
}
