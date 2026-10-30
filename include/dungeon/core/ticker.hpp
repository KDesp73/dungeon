#pragma once

#include <chrono>
#include <cstddef>

namespace dungeon::core {

/// Converts elapsed wall-clock time into fixed simulation ticks.
///
/// The ticker accumulates frame time and reports how many fixed-size updates
/// should run. Long stalls are clamped, and catch-up work is capped so a slow
/// frame cannot cause an unbounded run of updates.
class Ticker {
public:
    using Clock     = std::chrono::steady_clock;
    using Duration  = Clock::duration;
    using TimePoint = Clock::time_point;

    /// Creates a ticker with a fixed @p interval.
    ///
    /// @p max_elapsed limits how much time a single advance may account for.
    /// @p max_updates limits how many ticks one advance may return.
    ///
    /// @throws std::invalid_argument when @p interval or @p max_elapsed is not
    ///         positive, or when @p max_updates is zero.
    explicit Ticker(Duration interval,
                    Duration max_elapsed = std::chrono::milliseconds(250),
                    std::size_t max_updates = 5);

    /// Restarts the accumulator and uses @p now as the new reference time.
    void reset(TimePoint now = Clock::now()) noexcept;

    /// Advances the clock to @p now and returns how many ticks are due.
    ///
    /// If the clock moves backwards, no ticks are returned and the reference
    /// time is moved back to @p now.
    [[nodiscard]] std::size_t advance(TimePoint now = Clock::now()) noexcept;

    /// Returns the point in time when the next tick is due.
    [[nodiscard]] TimePoint next_tick() const noexcept;

    [[nodiscard]] Duration interval() const noexcept { return _interval; }
    [[nodiscard]] Duration remainder() const noexcept { return _accumulator; }

private:
    Duration _interval;
    Duration _max_elapsed;
    std::size_t _max_updates;
    TimePoint _last;
    Duration _accumulator{0};
};

} // namespace dungeon::core
