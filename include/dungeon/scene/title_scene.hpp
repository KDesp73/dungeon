#pragma once

#include "dungeon/scene/scene.hpp"

#include <functional>
#include <string>

namespace dungeon::scene {

/// Content and behaviour of the starting screen.
///
/// Every field has a sensible default, so `TitleScene{}` is already usable. Set
/// `on_start` to decide which scene replaces the title when the player starts.
struct TitleSceneConfig {
    /// Large text shown in the middle of the screen.
    std::string title = "DUNGEON";

    /// Optional second line, for example the version.
    std::string subtitle;

    /// Prompt shown below the title.
    std::string prompt = "Press ENTER to start";

    /// Optional hint shown under the prompt.
    std::string hint = "Esc to quit";

    /// Key that starts the game. Enter (CR or LF) is accepted by default.
    controls::KeyEvent start_key = controls::KeyEvent::from_character('\n');

    /// Called when the start key is pressed.
    std::function<void(SceneContext&)> on_start;

    /// When true, Escape requests that the game loop exits.
    bool quit_on_escape = true;
};

/// The first screen of the game.
///
/// It renders a title over the full framebuffer and starts the game through the
/// configured `on_start` callback.
class TitleScene final : public Scene {
  public:
    explicit TitleScene(TitleSceneConfig config = {});

    bool handle_key(const controls::KeyEvent& key, SceneContext& context) override;
    void draw(display::FrameBuffer& buffer, SceneContext& context) override;

    [[nodiscard]] const TitleSceneConfig& config() const noexcept { return _config; }

  private:
    [[nodiscard]] bool starts_game(const controls::KeyEvent& key) const noexcept;

    TitleSceneConfig _config;
};

} // namespace dungeon::scene
