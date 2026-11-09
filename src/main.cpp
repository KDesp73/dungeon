#include "dungeon/controls.hpp"
#include "dungeon/core/ticker.hpp"
#include "dungeon/core/version.hpp"
#include "dungeon/display/buffer.hpp"
#include "dungeon/display/term.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/scene/game_scene.hpp"
#include "dungeon/scene/scene_manager.hpp"
#include "dungeon/scene/title_scene.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#ifndef __EMSCRIPTEN__
#include "dungeon/map/map.hpp"

#include <CLI/CLI.hpp>

#include <csignal>
#include <iostream>
#include <thread>
#else
#include <emscripten/emscripten.h>
#endif

namespace {

#ifndef __EMSCRIPTEN__

volatile std::sig_atomic_t g_running = 1;

void handle_interrupt(int) {
    g_running = 0;
}

#endif

/// Owns the game state and advances it one frame at a time.
///
/// The native build drives it from a blocking loop; the browser build drives it
/// from an Emscripten main loop. Both share the same per-frame work.
class Application {
  public:
    explicit Application(std::optional<std::uint32_t> seed) : _ticker(tick_interval()) {
        const auto        layout = dungeon::display::tui::make_layout(_buffer.width(),
                                                                     _buffer.height());
        const std::size_t map_width  = layout.game.width > 2 ? layout.game.width - 2 : 0;
        const std::size_t map_height = layout.game.height > 2 ? layout.game.height - 2 : 0;

        dungeon::scene::TitleSceneConfig title;
        title.title    = "DUNGEON";
        title.subtitle =
            std::string(dungeon::core::kProjectName) + ' ' + std::string(dungeon::core::kVersion);
        title.prompt   = "Press ENTER to start";
        title.hint     = "Esc to quit";
        title.on_start = [map_width, map_height,
                          seed](dungeon::scene::SceneContext& scene_context) {
            dungeon::scene::GameSceneConfig config;
            config.width  = map_width;
            config.height = map_height;
            config.seed   = seed;

            scene_context.scenes.replace(
                std::make_unique<dungeon::scene::GameScene>(std::move(config)));
        };

        _scenes.push(std::make_unique<dungeon::scene::TitleScene>(std::move(title)));
    }

    /// Runs one frame and returns false once the game should stop.
    bool step() {
        const std::size_t updates = _ticker.advance();

        for (std::size_t i = 0; i < updates; ++i) {
            _scenes.update(_ticker.interval(), _context);
        }

        const dungeon::controls::KeyEvent key = dungeon::controls::poll_key();
        if (!key.is_none()) {
            static_cast<void>(_scenes.handle_key(key, _context));
        }

        if (_context.quit_requested) {
            return false;
        }

        _scenes.draw(_buffer, _context);
        _buffer.present();

        return true;
    }

    [[nodiscard]] dungeon::core::Ticker& ticker() noexcept {
        return _ticker;
    }

  private:
    static dungeon::core::Ticker::Duration tick_interval() {
        return std::chrono::duration_cast<dungeon::core::Ticker::Duration>(
            std::chrono::duration<double>(1.0 / 60.0));
    }

    dungeon::display::FrameBuffer _buffer;
    dungeon::scene::SceneManager  _scenes;
    dungeon::scene::SceneContext  _context{_scenes};
    dungeon::core::Ticker         _ticker;
};

} // namespace

int main(int argc, char** argv) {
    std::optional<std::uint32_t> seed_value;

#ifndef __EMSCRIPTEN__
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

    if (has_seed) {
        seed_value = seed;
    }

    std::signal(SIGINT, handle_interrupt);

    Application application(seed_value);

    const dungeon::controls::TerminalSession terminal;
    dungeon::display::tui::hide_cursor();

    while (g_running && application.step()) {
        std::this_thread::sleep_until(application.ticker().next_tick());
    }

    dungeon::display::tui::show_cursor();
#else
    // The browser supplies keys through `dungeon_push_key()` instead of argv
    // and a terminal, so there is nothing to parse here. The object is kept
    // alive for the lifetime of the page.
    auto* application = new Application(seed_value);

    emscripten_set_main_loop_arg(
        [](void* argument) {
            auto* game = static_cast<Application*>(argument);
            if (!game->step()) {
                emscripten_cancel_main_loop();
            }
        },
        application, 0, 1);
#endif

    return 0;
}
