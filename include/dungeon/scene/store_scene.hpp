#pragma once

#include "dungeon/game.hpp"
#include "dungeon/scene/scene.hpp"

#include <cstddef>
#include <string>

namespace dungeon::scene {

/// Content and behaviour of the store level.
struct StoreSceneConfig {
    /// Heading shown at the top of the store panel.
    std::string title = "STORE";

    /// Hint shown at the bottom of the store panel.
    std::string footer = "Up/Down select   Enter buy   Esc leave";
};

/// The shop that appears every few dungeons.
///
/// The scene reads the catalog, gold, and stats from the `Game` it is given and
/// buys upgrades through `Game::buy_store_item()`. It draws over the frozen
/// dungeon and closes itself when the player leaves, which also marks the store
/// visit as finished.
class StoreScene final : public Scene {
  public:
    explicit StoreScene(game::Game& game, StoreSceneConfig config = {});

    bool handle_key(const controls::KeyEvent& key, SceneContext& context) override;
    void draw(display::FrameBuffer& buffer, SceneContext& context) override;
    void on_exit() override;

    [[nodiscard]] bool draws_underlying() const noexcept override { return true; }

    /// Index of the highlighted store item.
    [[nodiscard]] std::size_t selected() const noexcept { return _selected; }

    /// Last purchase result shown under the catalog.
    [[nodiscard]] const std::string& status() const noexcept { return _status; }

  private:
    void move_selection(int delta) noexcept;
    void buy();

    game::Game&      _game;
    StoreSceneConfig _config;
    std::size_t      _selected = 0;
    std::string      _status;
};

} // namespace dungeon::scene
