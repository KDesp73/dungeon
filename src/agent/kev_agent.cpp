#include "agent/kev_agent.hpp"

#include <utility>

namespace dungeon::agent {

KevAgent::KevAgent(std::string url, std::unique_ptr<KevTransport> transport,
                   std::size_t fallback_interval)
    : _transport(transport != nullptr ? std::move(transport)
                                      : std::make_unique<HttpKevClient>(std::move(url))),
      _fallback_interval(fallback_interval == 0 ? 1 : fallback_interval),
      _worker([this]() { worker(); }) {}

KevAgent::~KevAgent() {
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _stop = true;
    }
    _cv.notify_all();
    if (_worker.joinable()) {
        _worker.join();
    }
}

void KevAgent::observe(const game::Game& game, const scene::SceneManager& scenes) {
    const AgentObservation observation = AgentObservation::from(game, scenes);
    std::size_t id                     = 0;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        id           = ++_next_id;
        _current_id  = id;
        _last        = observation;
    }

    const std::string body = build_kev_request(observation, id);
    {
        std::lock_guard<std::mutex> lock(_mutex);
        // Only the newest observation matters. Drop anything not yet sent.
        _pending.clear();
        _pending.push_back({id, body});
    }
    _cv.notify_one();
}

std::optional<AgentDecision> KevAgent::poll() {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_result) {
        const Result result = *_result;
        _result.reset();
        if (result.id != _current_id) {
            ++_stale_replies;
            return std::nullopt;
        }
        if (_last) {
            _ticks = 0;
            return decide(*_last, result.answers, &_memory);
        }
        return std::nullopt;
    }

    if (_last && ++_ticks >= _fallback_interval) {
        _ticks = 0;
        return decide(*_last, KevAnswers{}, &_memory);
    }
    return std::nullopt;
}

std::size_t KevAgent::current_id() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _current_id;
}

std::size_t KevAgent::stale_replies() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _stale_replies;
}

void KevAgent::worker() {
    while (true) {
        Request request;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cv.wait(lock, [this]() { return _stop || !_pending.empty(); });
            if (_pending.empty()) {
                if (_stop) {
                    return;
                }
                continue;
            }
            request = std::move(_pending.front());
            _pending.pop_front();
        }

        const KevTransport::Reply reply = _transport->post(request.body);
        KevAnswers answers;
        if (!reply.ok || !parse_kev_response(reply.body, answers)) {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(_mutex);
            _result = Result{request.id, answers};
        }
    }
}

} // namespace dungeon::agent
