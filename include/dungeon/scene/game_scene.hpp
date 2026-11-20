#pragma once

#include "dungeon/game.hpp"
#include "dungeon/save/save.hpp"
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
                                 "Take item: Enter\n"
                                 "Store:     approach the shopkeeper\n"
#ifdef DUNGEON_DEV
                                 "Dev:       @ jump to next store/boss\n"
#endif
                                 "Shop:      Enter buy, Esc leave";

    /// Called when the player chooses "Save game" in the pause menu.
    ///
    /// When empty, the game is written to @ref save_path using the binary save
    /// format. A custom handler is useful for tests or alternative storage.
    SaveHandler save_handler;

    /// File used by the built-in save and load actions.
    std::string save_path = save::kDefaultSavePath;
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

    /// Loads the configured save file into this scene's game.
    ///
    /// The scene keeps ownership of the loaded `Game`, so this works before the
    /// scene is pushed as well as from the pause menu.
    [[nodiscard]] save::SaveResult load_game();

  private:
    void        open_pause_menu(SceneContext& context);
    void        update_store_proximity(SceneContext& context);
    std::string save_game();

#ifdef DUNGEON_DEV
    /// Dev-only shortcut: descend to the next store or boss level.
    void jump_to_special_level(SceneContext& context);
#endif

    GameSceneConfig _config;
    game::Game      _game;
    bool            _store_near = false;
};

} // namespace dungeon::scene
