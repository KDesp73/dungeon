#pragma once

#include "dungeon/scene/scene.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace dungeon::scene {

/// One selectable entry in a pause menu.
///
/// `action` returns an optional status line. Return an empty string to leave the
/// current status unchanged.
struct PauseOption {
    std::string                               label;
    std::function<std::string(SceneContext&)> action;
};

/// Content and behaviour of the pause menu.
struct PauseSceneConfig {
    /// Heading shown at the top of the menu.
    std::string title = "PAUSED";

    /// Multi-line control reference. Each `\n` starts a new line.
    std::string controls;

    /// Entries shown below the controls.
    std::vector<PauseOption> options;

    /// Hint shown at the bottom of the menu.
    std::string footer = "Up/Down select   Enter confirm   Esc resume";

    /// When true, Escape closes the pause scene.
    bool resume_on_escape = true;
};

/// A menu overlay that freezes the scene underneath and draws on top of it.
///
/// Use the Up/Down or W/S keys to move the selection and Enter/Space to run the
/// selected action. The scene does not update the scene below it, which pauses
/// the game simulation while the menu is open.
class PauseScene final : public Scene {
  public:
    explicit PauseScene(PauseSceneConfig config = {});

    bool handle_key(const controls::KeyEvent& key, SceneContext& context) override;
    void draw(display::FrameBuffer& buffer, SceneContext& context) override;

    [[nodiscard]] bool draws_underlying() const noexcept override { return true; }

    /// Index of the highlighted option.
    [[nodiscard]] std::size_t selected() const noexcept { return _selected; }

    /// Last non-empty status returned by an option action.
    [[nodiscard]] const std::string& status() const noexcept { return _status; }

  private:
    void move_selection(int delta) noexcept;
    void activate(SceneContext& context);

    PauseSceneConfig _config;
    std::size_t      _selected = 0;
    std::string      _status;
};

} // namespace dungeon::scene
