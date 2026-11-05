#include "dungeon/scene/scene_manager.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace dungeon::scene {

namespace {

/// Marks a dispatch as in progress so scene changes are queued instead of
/// mutating the stack while a scene method is running.
class DispatchGuard {
  public:
    explicit DispatchGuard(bool& flag) : _flag(flag) { _flag = true; }
    ~DispatchGuard() { _flag = false; }

    DispatchGuard(const DispatchGuard&)            = delete;
    DispatchGuard& operator=(const DispatchGuard&) = delete;

  private:
    bool& _flag;
};

} // namespace

SceneManager::~SceneManager() {
    // Queue any scene changes requested by on_exit() instead of applying them.
    _dispatching = true;

    while (!_stack.empty()) {
        _stack.back()->on_exit();
        _stack.pop_back();
    }

    _pending.clear();
}

void SceneManager::push(std::unique_ptr<Scene> scene) {
    enqueue(Operation::Push, std::move(scene));
}

void SceneManager::pop() {
    enqueue(Operation::Pop, nullptr);
}

void SceneManager::replace(std::unique_ptr<Scene> scene) {
    enqueue(Operation::Replace, std::move(scene));
}

void SceneManager::clear() {
    enqueue(Operation::Clear, nullptr);
}

void SceneManager::apply_pending() {
    if (_applying) {
        return;
    }

    const DispatchGuard guard(_applying);

    while (!_pending.empty()) {
        Change change = std::move(_pending.front());
        _pending.erase(_pending.begin());

        switch (change.operation) {
        case Operation::Push:
            if (change.scene) {
                change.scene->on_enter();
                _stack.push_back(std::move(change.scene));
            }
            break;
        case Operation::Pop:
            if (!_stack.empty()) {
                _stack.back()->on_exit();
                _stack.pop_back();
            }
            break;
        case Operation::Replace:
            if (!_stack.empty()) {
                _stack.back()->on_exit();
                _stack.pop_back();
            }
            if (change.scene) {
                change.scene->on_enter();
                _stack.push_back(std::move(change.scene));
            }
            break;
        case Operation::Clear:
            while (!_stack.empty()) {
                _stack.back()->on_exit();
                _stack.pop_back();
            }
            break;
        }
    }
}

void SceneManager::update(Duration delta, SceneContext& context) {
    apply_pending();

    if (_stack.empty()) {
        return;
    }

    std::size_t first = _stack.size() - 1;
    while (first > 0 && _stack[first]->updates_underlying()) {
        --first;
    }

    std::vector<Scene*> active;
    active.reserve(_stack.size() - first);
    for (std::size_t index = first; index < _stack.size(); ++index) {
        active.push_back(_stack[index].get());
    }

    {
        const DispatchGuard guard(_dispatching);
        for (Scene* scene : active) {
            scene->update(delta, context);
        }
    }

    apply_pending();
}

bool SceneManager::handle_key(const controls::KeyEvent& key, SceneContext& context) {
    apply_pending();

    if (_stack.empty()) {
        return false;
    }

    Scene* scene   = _stack.back().get();
    bool   handled = false;
    {
        const DispatchGuard guard(_dispatching);
        handled = scene->handle_key(key, context);
    }

    apply_pending();
    return handled;
}

void SceneManager::draw(display::FrameBuffer& buffer, SceneContext& context) {
    apply_pending();

    if (_stack.empty()) {
        return;
    }

    std::size_t first = _stack.size() - 1;
    while (first > 0 && _stack[first]->draws_underlying()) {
        --first;
    }

    std::vector<Scene*> active;
    active.reserve(_stack.size() - first);
    for (std::size_t index = first; index < _stack.size(); ++index) {
        active.push_back(_stack[index].get());
    }

    {
        const DispatchGuard guard(_dispatching);
        for (Scene* scene : active) {
            scene->draw(buffer, context);
        }
    }

    apply_pending();
}

void SceneManager::enqueue(Operation operation, std::unique_ptr<Scene> scene) {
    _pending.push_back(Change{operation, std::move(scene)});

    if (!_dispatching && !_applying) {
        apply_pending();
    }
}

} // namespace dungeon::scene
