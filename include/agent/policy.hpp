#pragma once

#include "agent/agent.hpp"

#include <deque>
#include <map>
#include <string>
#include <vector>

namespace dungeon::agent {

/// One action the game code allows in the current observation.
struct LegalAction {
    std::string             id;
    std::string             description;
    controls::KeyEvent      key;
};

/// Enumerates the actions that the policy may emit for @p observation.
///
/// Kev is only ever offered these ids, so a reply naming anything else is
/// discarded by `decide()`.
[[nodiscard]] std::vector<LegalAction> legal_actions(const AgentObservation& observation);

/// Typed answers returned by Kev. Defaults are "no answer" values; `received`
/// tells the policy whether to use them at all.
struct KevAnswers {
    struct Choice {
        std::string                      choice;
        double                           confidence = 0.0;
        std::map<std::string, double>    probabilities;
    };

    struct Noul {
        double noul = 0.5;
    };

    struct Score {
        double score      = 0.0;
        double confidence = 0.0;
    };

    Choice goal;
    Noul   engage;
    Score  danger;
    Choice move;
    Choice buy;
    Choice chest;
    bool   received = false;
};

/// Short movement history used to detect and break A -> B -> A -> B loops.
///
/// Agents own one of these and pass it to `decide()`. The policy stays pure when
/// no memory is supplied (as in tests and one-shot callers).
class DecisionMemory {
  public:
    /// Remembers that a decision left @p from by moving in direction @p kind.
    void record(AgentObservation::Cell from, controls::KeyKind kind);

    /// True when the last few moves revisit the same cells over and over.
    [[nodiscard]] bool stalled() const noexcept;

    /// Least-visited legal move away from the current loop, if stalled.
    [[nodiscard]] std::optional<controls::KeyKind>
    break_move(const AgentObservation& observation,
               const std::vector<LegalAction>& actions) const;

    /// Forgets the history after a loop has been broken.
    void clear() noexcept;

  private:
    std::deque<AgentObservation::Cell> _cells;
    std::deque<controls::KeyKind>      _moves;
};

/// Combines Kev's answers with deterministic rules and returns legal keys.
///
/// Passing a default-constructed `KevAnswers` runs the scripted fallback. When
/// @p memory is supplied, repeated back-and-forth movement is broken by taking
/// the least-recently-visited legal step instead of the requested move.
[[nodiscard]] AgentDecision decide(const AgentObservation& observation, const KevAnswers& answers,
                                   DecisionMemory* memory = nullptr);

/// First step on a path toward @p target, avoiding observed hazards when
/// possible. Used by both the scripted bot and the fallback policy.
[[nodiscard]] std::optional<controls::KeyEvent> step_toward(const AgentObservation& observation,
                                                            AgentObservation::Cell target);

/// Parses a Kev `/v1/systemone` response body.
///
/// Returns false when the body is malformed; the caller then falls back to the
/// scripted policy. Unknown option names are kept but never trusted by
/// `decide()`.
[[nodiscard]] bool parse_kev_response(const std::string& body, KevAnswers& answers);

/// Builds the questions for one observation, matchable by `parse_kev_response`.
[[nodiscard]] std::string build_kev_request(const AgentObservation& observation,
                                             std::size_t observation_id);

} // namespace dungeon::agent
