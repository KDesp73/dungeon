#pragma once

#include "dungeon/controls.hpp"
#include "dungeon/display/buffer.hpp"

#include <chrono>

namespace dungeon::scene {

class SceneManager;

/// Simulation time passed to `Scene::update()`.
using Duration = std::chrono::steady_clock::duration;

/// Services and shared state available to every active scene.
///
/// The context is passed to `update()`, `handle_key()`, and `draw()` instead of
/// being stored in a scene, so scenes stay easy to construct and test.
struct SceneContext {
    /// The manager a scene can use to push, pop, or replace scenes.
    SceneManager& scenes;

    /// Set to true to leave the game loop after the current frame.
    bool quit_requested = false;
};

/// One screen or overlay in the game.
///
/// Scenes are ordinary objects owned by a `SceneManager`. Override the hooks
/// you need; only `draw()` is required. A scene may request that the scene
/// below it keeps updating or is drawn first, which is how the pause menu shows
/// the frozen game underneath it.
class Scene {
  public:
    virtual ~Scene() = default;

    /// Called once when the scene is pushed or becomes the top of the stack.
    virtual void on_enter() {}

    /// Called once when the scene is popped or replaced.
    virtual void on_exit() {}

    /// Advances the scene by @p delta simulation time.
    virtual void update(Duration /*delta*/, SceneContext& /*context*/) {}

    /// Handles @p key. Returns true when the scene consumed it.
    ///
    /// Only the top scene receives input.
    virtual bool handle_key(const controls::KeyEvent& /*key*/, SceneContext& /*context*/) {
        return false;
    }

    /// Draws the scene into @p buffer.
    virtual void draw(display::FrameBuffer& /*buffer*/, SceneContext& /*context*/) = 0;

    /// When true, the scene underneath continues to receive updates.
    ///
    /// The default freezes everything below this scene.
    [[nodiscard]] virtual bool updates_underlying() const noexcept { return false; }

    /// When true, the scene underneath is drawn before this one.
    ///
    /// Enable this for overlays such as a pause menu.
    [[nodiscard]] virtual bool draws_underlying() const noexcept { return false; }
};

} // namespace dungeon::scene
