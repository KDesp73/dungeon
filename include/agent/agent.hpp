#pragma once

#include "agent/observation.hpp"
#include "dungeon/controls.hpp"

#include <optional>
#include <vector>

namespace dungeon::agent {

/// Keys an agent wants the game loop to inject, in order.
struct AgentDecision {
    std::vector<controls::KeyEvent> keys;
};

/// A driver that observes the game at a low cadence and produces key events.
///
/// Implementations never mutate `Game` themselves; the keys they return flow
/// through `SceneManager::handle_key()` like human input.
class Agent {
  public:
    virtual ~Agent() = default;

    virtual void observe(const game::Game& game, const scene::SceneManager& scenes) = 0;

    /// Returns a decision at most once per call.
    [[nodiscard]] virtual std::optional<AgentDecision> poll() = 0;
};

} // namespace dungeon::agent
