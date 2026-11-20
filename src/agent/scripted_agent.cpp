#include "agent/scripted_agent.hpp"

#include "agent/policy.hpp"

namespace dungeon::agent {

void ScriptedAgent::observe(const game::Game& game, const scene::SceneManager& scenes) {
    const AgentObservation observation = AgentObservation::from(game, scenes);
    _decision                          = decide(observation, KevAnswers{});
}

std::optional<AgentDecision> ScriptedAgent::poll() {
    const std::optional<AgentDecision> decision = _decision;
    _decision.reset();
    return decision;
}

} // namespace dungeon::agent
