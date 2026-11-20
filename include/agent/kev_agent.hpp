#pragma once

#include "agent/agent.hpp"
#include "agent/kev_client.hpp"
#include "agent/policy.hpp"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace dungeon::agent {

/// Asynchronous Kev driver.
///
/// `observe()` snapshots the game and queues a request for a worker thread;
/// `poll()` never performs network I/O. Stale replies are discarded and a
/// deterministic scripted decision is emitted periodically while a request is
/// in flight.
class KevAgent final : public Agent {
  public:
    explicit KevAgent(std::string url, std::unique_ptr<KevTransport> transport = nullptr,
                      std::size_t fallback_interval = 12);
    ~KevAgent() override;

    KevAgent(const KevAgent&)            = delete;
    KevAgent& operator=(const KevAgent&) = delete;

    void observe(const game::Game& game, const scene::SceneManager& scenes) override;
    [[nodiscard]] std::optional<AgentDecision> poll() override;

    [[nodiscard]] std::size_t current_id() const;
    [[nodiscard]] std::size_t stale_replies() const;

  private:
    struct Request {
        std::size_t id;
        std::string body;
    };

    struct Result {
        std::size_t id;
        KevAnswers  answers;
    };

    void worker();

    std::unique_ptr<KevTransport> _transport;
    std::size_t                   _fallback_interval;
    std::thread                   _worker;
    mutable std::mutex            _mutex;
    std::condition_variable       _cv;
    std::deque<Request>           _pending;
    std::optional<Result>         _result;
    std::optional<AgentObservation> _last;
    DecisionMemory                _memory;
    std::size_t                   _current_id    = 0;
    std::size_t                   _next_id       = 0;
    std::size_t                   _stale_replies = 0;
    std::size_t                   _ticks         = 0;
    bool                          _stop         = false;
};

} // namespace dungeon::agent
