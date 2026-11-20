#include "agent/agent.hpp"
#include "agent/scripted_agent.hpp"
#include "dungeon/controls.hpp"
#include "dungeon/controls/input_source.hpp"
#include "dungeon/core/ticker.hpp"
#include "dungeon/core/version.hpp"
#include "dungeon/display/buffer.hpp"
#include "dungeon/display/term.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/save/save.hpp"
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
#include "agent/kev_agent.hpp"
#include "dungeon/map/map.hpp"

#include <CLI/CLI.hpp>

#include <csignal>
#include <iostream>
#include <thread>
#else
#include <emscripten/emscripten.h>
#endif

namespace {

enum class AgentKind : unsigned char {
    None,
    Scripted,
    Kev,
};

struct AgentOptions {
    AgentKind   kind      = AgentKind::None;
    std::string kev_url   = "http://127.0.0.1:8008";
    int         interval  = 12;
    bool        headless  = false;
    bool        verbose   = false;
};

#ifndef __EMSCRIPTEN__

const char* key_name(const dungeon::controls::KeyEvent& key) noexcept {
    using dungeon::controls::KeyKind;
    switch (key.kind) {
    case KeyKind::None:
        return "none";
    case KeyKind::Character:
        return "char";
    case KeyKind::Up:
        return "up";
    case KeyKind::Down:
        return "down";
    case KeyKind::Left:
        return "left";
    case KeyKind::Right:
        return "right";
    case KeyKind::Escape:
        return "escape";
    }
    return "unknown";
}

void log_key(const dungeon::controls::KeyEvent& key) {
    if (key.kind == dungeon::controls::KeyKind::Character) {
        std::cerr << (key.value == '\n' ? "enter" : std::string(1, key.value));
    } else {
        std::cerr << key_name(key);
    }
}

#endif // __EMSCRIPTEN__

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
    explicit Application(std::optional<std::uint32_t> seed,
                         std::string save_path = dungeon::save::kDefaultSavePath,
                         AgentOptions options = {})
        : _options(options), _ticker(tick_interval()) {
        // A headless run has no terminal to size the map from, so fall back to
        // the same dimensions the game generator uses by default.
        constexpr std::size_t kHeadlessMapWidth  = 64;
        constexpr std::size_t kHeadlessMapHeight = 32;
        std::size_t           map_width          = kHeadlessMapWidth;
        std::size_t           map_height         = kHeadlessMapHeight;
        if (!options.headless) {
            const auto layout = dungeon::display::tui::make_layout(_buffer.width(),
                                                                   _buffer.height());
            if (layout.game.width > 2) {
                map_width = layout.game.width - 2;
            }
            if (layout.game.height > 2) {
                map_height = layout.game.height - 2;
            }
#ifndef __EMSCRIPTEN__
            // make_spec() needs room for a 3x3 room grid. Without it the map is
            // empty and there is nothing for an agent (or a player) to explore.
            if (map_width < 28 || map_height < 22) {
                std::cerr << "warning: terminal viewport is " << map_width << 'x' << map_height
                          << ", but map generation needs at least 28x22; "
                             "resize the terminal (about 30 rows) or use --headless\n";
            }
#endif
        }

        dungeon::scene::TitleSceneConfig title;
        title.title    = "DUNGEON";
        title.subtitle =
            std::string(dungeon::core::kProjectName) + ' ' + std::string(dungeon::core::kVersion);
        title.prompt   = "Press ENTER to start";
        title.hint     = "Esc to quit";
        title.on_start = [map_width, map_height, seed,
                          save_path](dungeon::scene::SceneContext& scene_context) {
            dungeon::scene::GameSceneConfig config;
            config.width     = map_width;
            config.height    = map_height;
            config.seed      = seed;
            config.save_path = save_path;

            scene_context.scenes.replace(
                std::make_unique<dungeon::scene::GameScene>(std::move(config)));
        };
        title.on_load = [map_width, map_height, seed,
                         save_path](dungeon::scene::SceneContext& scene_context) {
            dungeon::scene::GameSceneConfig config;
            config.width     = map_width;
            config.height    = map_height;
            config.seed      = seed;
            config.save_path = save_path;

            auto scene = std::make_unique<dungeon::scene::GameScene>(std::move(config));
            if (!scene->load_game().ok) {
                return false;
            }

            scene_context.scenes.replace(std::move(scene));
            return true;
        };

        _scenes.push(std::make_unique<dungeon::scene::TitleScene>(std::move(title)));

        if (!options.headless) {
            _input = std::make_unique<dungeon::controls::TerminalKeySource>();
        }

#ifndef __EMSCRIPTEN__
        if (options.kind == AgentKind::Scripted) {
            _agent = std::make_unique<dungeon::agent::ScriptedAgent>();
        } else if (options.kind == AgentKind::Kev) {
#ifdef DUNGEON_ENABLE_KEV
            _agent = std::make_unique<dungeon::agent::KevAgent>(
                options.kev_url, nullptr, static_cast<std::size_t>(options.interval));
#else
            std::cerr << "--agent kev requires DUNGEON_ENABLE_KEV\n";
#endif
        }
#endif
    }

    /// Runs one frame and returns false once the game should stop.
    bool step() {
        const std::size_t updates = _ticker.advance();

        for (std::size_t i = 0; i < updates; ++i) {
            _scenes.update(_ticker.interval(), _context);
        }

        if (_agent) {
            if (!_auto_started) {
                _injected.push(dungeon::controls::KeyEvent::from_character('\n'));
                _auto_started = true;
            }

            ++_tick;
            if (_tick % static_cast<std::size_t>(_options.interval) == 0) {
                const auto* game_scene = _scenes.find<dungeon::scene::GameScene>();
                if (game_scene != nullptr) {
                    _agent->observe(game_scene->game(), _scenes);
#ifndef __EMSCRIPTEN__
                    if (_options.verbose) {
                        const auto& game = game_scene->game();
                        const auto* player = game.player();
                        std::cerr << "[agent] observe depth=" << game.depth() << " hp="
                                  << (player != nullptr ? player->stats().hp : 0)
                                  << " gold=" << (player != nullptr ? player->stats().gold : 0)
                                  << " enemies=" << (game.nearest_enemy() != nullptr ? 1 : 0)
                                  << " key=" << (player != nullptr && player->has_key() ? 1 : 0)
                                  << " at=" << (player != nullptr ? player->x : 0) << ","
                                  << (player != nullptr ? player->y : 0)
                                  << '\n';
                    }
#endif
                }
            }
            if (const auto decision = _agent->poll()) {
#ifndef __EMSCRIPTEN__
                if (_options.verbose && !decision->keys.empty()) {
                    std::cerr << "[agent] -> ";
                    for (std::size_t i = 0; i < decision->keys.size(); ++i) {
                        if (i != 0) {
                            std::cerr << ' ';
                        }
                        log_key(decision->keys[i]);
                    }
                    std::cerr << '\n';
                }
#endif
                for (const dungeon::controls::KeyEvent key : decision->keys) {
                    _injected.push(key);
                }
            }
        }

        for (int i = 0; i < 16; ++i) {
            dungeon::controls::KeyEvent key = _injected.poll();
            if (key.is_none() && _input) {
                key = _input->poll();
            }
            if (key.is_none()) {
                break;
            }
            static_cast<void>(_scenes.handle_key(key, _context));
        }

        if (_context.quit_requested) {
            return false;
        }

        if (!_options.headless) {
            _scenes.draw(_buffer, _context);
            _buffer.present();
        }

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

    AgentOptions                              _options;
    dungeon::display::FrameBuffer             _buffer;
    dungeon::scene::SceneManager              _scenes;
    dungeon::scene::SceneContext              _context{_scenes};
    dungeon::core::Ticker                     _ticker;
    std::unique_ptr<dungeon::controls::KeySource> _input;
    dungeon::controls::QueuedKeySource        _injected;
    std::unique_ptr<dungeon::agent::Agent>    _agent;
    std::size_t                               _tick         = 0;
    bool                                      _auto_started = false;
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
    std::string save_path = dungeon::save::kDefaultSavePath;
    app.add_option("--save-file", save_path, "Path used by the in-game save and load actions");
    bool print_map = false;
    app.add_flag("-p,--print-map", print_map, "Print a generated dungeon map and exit");

    std::string agent_name = "none";
    app.add_option("--agent", agent_name, "Agent driver: none, scripted, or kev")
        ->check(CLI::IsMember({"none", "scripted", "kev"}));
    std::string kev_url = "http://127.0.0.1:8008";
    app.add_option("--kev-url", kev_url, "Kev endpoint URL");
    int kev_interval = 12;
    app.add_option("--kev-interval", kev_interval, "Observation cadence in ticks")
        ->check(CLI::PositiveNumber);
    bool headless = false;
    app.add_flag("--headless", headless, "Run without terminal rendering (evaluation)");
    bool verbose = false;
    app.add_flag("--verbose", verbose, "Log agent observations and decisions to stderr");

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

    AgentOptions agent_options;
    if (agent_name == "scripted") {
        agent_options.kind = AgentKind::Scripted;
    } else if (agent_name == "kev") {
#ifndef DUNGEON_ENABLE_KEV
        std::cerr << "--agent kev is unavailable: built with DUNGEON_ENABLE_KEV=OFF\n";
        return 1;
#else
        agent_options.kind    = AgentKind::Kev;
        agent_options.kev_url = kev_url;
        agent_options.interval = kev_interval;
#endif
    }
    agent_options.headless = headless;
    agent_options.verbose  = verbose;

    std::signal(SIGINT, handle_interrupt);

    Application application(seed_value, save_path, agent_options);

    std::optional<dungeon::controls::TerminalSession> terminal;
    if (!headless) {
        terminal.emplace();
        dungeon::display::tui::hide_cursor();
    }

    while (g_running && application.step()) {
        std::this_thread::sleep_until(application.ticker().next_tick());
    }

    if (!headless) {
        dungeon::display::tui::show_cursor();
    }
#else
    static_cast<void>(argc);
    static_cast<void>(argv);

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
