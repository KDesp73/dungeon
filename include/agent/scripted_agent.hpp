#pragma once

#include "agent/agent.hpp"
#include "agent/policy.hpp"

#include <optional>

namespace dungeon::agent {

/// BFS-and-rules baseline. Never talks to a model and emits only legal keys.
class ScriptedAgent final : public Agent {
  public:
    void observe(const game::Game& game, const scene::SceneManager& scenes) override;
    [[nodiscard]] std::optional<AgentDecision> poll() override;

  private:
    std::optional<AgentDecision> _decision;
    DecisionMemory               _memory;
};

} // namespace dungeon::agent
