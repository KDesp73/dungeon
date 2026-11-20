#pragma once

#include "dungeon/controls.hpp"

#include <deque>

namespace dungeon::controls {

/// Source of key events consumed by the game loop once per tick.
class KeySource {
  public:
    virtual ~KeySource() = default;

    /// Returns the next key, or a `None` event when nothing is available.
    [[nodiscard]] virtual KeyEvent poll() = 0;
};

/// Reads keys from the real terminal.
class TerminalKeySource final : public KeySource {
  public:
    [[nodiscard]] KeyEvent poll() override { return poll_key(); }
};

/// FIFO of keys injected by an agent or a test.
class QueuedKeySource final : public KeySource {
  public:
    void push(KeyEvent key) { _keys.push_back(key); }

    [[nodiscard]] KeyEvent poll() override {
        if (_keys.empty()) {
            return {};
        }
        const KeyEvent key = _keys.front();
        _keys.pop_front();
        return key;
    }

    [[nodiscard]] bool empty() const noexcept { return _keys.empty(); }

  private:
    std::deque<KeyEvent> _keys;
};

} // namespace dungeon::controls
