#pragma once

#include "dungeon/game.hpp"
#include "dungeon/scene/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace dungeon::scene {

/// Optional callback used by the pause menu to persist the current run.
///
/// Returning true reports "Game saved." in the menu; false reports a failure.
using SaveHandler = std::function<bool(const game::Game&)>;

/// Configuration for the playable scene.
struct GameSceneConfig {
    /// Build this exact layout instead of a random dungeon.
    std::optional<map::MapSpec> layout;

    /// Dimensions used when no explicit layout is supplied.
    std::size_t width  = 64;
    std::size_t height = 32;

    /// Seed for the generated dungeon. When empty a random seed is used.
    std::optional<std::uint32_t> seed;

    /// Heading shown by the pause menu.
    std::string pause_title = "PAUSED";

    /// Control reference shown by the pause menu.
    std::string pause_controls = "Move:      W A S D / Arrow keys\n"
                                 "Pause:     Esc\n"
                                 "Attack:    move into an enemy\n"
                                 "Potion:    H use health potion\n"
                                 "Chest:     [ ] cycle, 1-9 select\n"
                                 "Take item: Enter";

    /// Called when the player chooses "Save game" in the pause menu.
    SaveHandler save_handler;
};

/// The playable dungeon scene.
///
/// It owns the `Game`, renders the existing dungeon UI, and opens a `PauseScene`
/// when Escape is pressed.
class GameScene final : public Scene {
  public:
    explicit GameScene(GameSceneConfig config = {});

    bool handle_key(const controls::KeyEvent& key, SceneContext& context) override;
    void update(Duration delta, SceneContext& context) override;
    void draw(display::FrameBuffer& buffer, SceneContext& context) override;

    [[nodiscard]] game::Game&       game() noexcept { return _game; }
    [[nodiscard]] const game::Game& game() const noexcept { return _game; }

    [[nodiscard]] const GameSceneConfig& config() const noexcept { return _config; }

    /// Replaces the pause menu's save callback.
    void set_save_handler(SaveHandler handler) { _config.save_handler = std::move(handler); }

  private:
    void        open_pause_menu(SceneContext& context);
    std::string save_game();

    GameSceneConfig _config;
    game::Game      _game;
};

} // namespace dungeon::scene
