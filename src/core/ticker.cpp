#include "dungeon/core/ticker.hpp"

#include <stdexcept>

namespace dungeon::core {

Ticker::Ticker(Duration interval, Duration max_elapsed, std::size_t max_updates)
    : _interval(interval),
      _max_elapsed(max_elapsed),
      _max_updates(max_updates),
      _last(Clock::now())
{
    if (_interval <= Duration::zero()) {
        throw std::invalid_argument("Ticker: interval must be positive");
    }

    if (_max_elapsed <= Duration::zero()) {
        throw std::invalid_argument("Ticker: max_elapsed must be positive");
    }

    if (_max_updates == 0) {
        throw std::invalid_argument("Ticker: max_updates must be greater than zero");
    }
}

void Ticker::reset(TimePoint now) noexcept
{
    _last        = now;
    _accumulator = Duration::zero();
}

std::size_t Ticker::advance(TimePoint now) noexcept
{
    if (now < _last) {
        _last = now;
        return 0;
    }

    Duration elapsed = now - _last;
    _last            = now;

    if (elapsed > _max_elapsed) {
        elapsed = _max_elapsed;
    }

    _accumulator += elapsed;

    std::size_t updates = 0;
    while (_accumulator >= _interval && updates < _max_updates) {
        _accumulator -= _interval;
        ++updates;
    }

    // Drop the remaining backlog when catch-up work has been capped. Keeping
    // it would make every following frame try to catch up again.
    if (updates == _max_updates && _accumulator >= _interval) {
        _accumulator = Duration::zero();
    }

    return updates;
}

Ticker::TimePoint Ticker::next_tick() const noexcept
{
    return _last + (_interval - _accumulator);
}

} // namespace dungeon::core
