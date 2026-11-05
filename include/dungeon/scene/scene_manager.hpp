#pragma once

#include "dungeon/scene/scene.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace dungeon::scene {

/// Owns a stack of scenes and dispatches lifecycle, input, update, and draw
/// calls to them.
///
/// Scene changes requested while a scene is running are queued and applied
/// after the current dispatch finishes. That makes it safe for a scene to pop
/// itself or push a new scene from `handle_key()` or `update()`.
class SceneManager {
  public:
    SceneManager() = default;
    ~SceneManager();

    SceneManager(const SceneManager&)            = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    SceneManager(SceneManager&&)                 = delete;
    SceneManager& operator=(SceneManager&&)      = delete;

    /// Adds @p scene on top of the stack and calls its `on_enter()`.
    ///
    /// A null pointer is ignored.
    void push(std::unique_ptr<Scene> scene);

    /// Removes the top scene and calls its `on_exit()`.
    ///
    /// Does nothing when the stack is empty.
    void pop();

    /// Replaces the top scene with @p scene.
    ///
    /// Behaves like `push()` when the stack is empty and like `pop()` when
    /// @p scene is null.
    void replace(std::unique_ptr<Scene> scene);

    /// Removes every scene, calling `on_exit()` from top to bottom.
    void clear();

    /// Applies scene changes queued during the current dispatch.
    ///
    /// `push()`, `pop()`, `replace()`, and `clear()` already apply immediately
    /// outside a dispatch, so callers normally do not need this.
    void apply_pending();

    [[nodiscard]] bool        empty() const noexcept { return _stack.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return _stack.size(); }

    [[nodiscard]] Scene* top() noexcept { return _stack.empty() ? nullptr : _stack.back().get(); }
    [[nodiscard]] const Scene* top() const noexcept {
        return _stack.empty() ? nullptr : _stack.back().get();
    }

    /// Updates the top scene and any underlying scenes it keeps alive.
    void update(Duration delta, SceneContext& context);

    /// Sends @p key to the top scene and returns whether it was consumed.
    bool handle_key(const controls::KeyEvent& key, SceneContext& context);

    /// Draws the top scene and any underlying scenes it is layered over.
    void draw(display::FrameBuffer& buffer, SceneContext& context);

  private:
    enum class Operation : unsigned char {
        Push,
        Pop,
        Replace,
        Clear,
    };

    struct Change {
        Operation              operation = Operation::Push;
        std::unique_ptr<Scene> scene;
    };

    void enqueue(Operation operation, std::unique_ptr<Scene> scene);

    std::vector<std::unique_ptr<Scene>> _stack;
    std::vector<Change>                 _pending;
    bool                                _dispatching = false;
    bool                                _applying    = false;
};

} // namespace dungeon::scene
