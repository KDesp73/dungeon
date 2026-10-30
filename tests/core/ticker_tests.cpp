#include "test_harness.hpp"

#include "dungeon/core/ticker.hpp"

#include <chrono>
#include <stdexcept>

using dungeon::core::Ticker;

namespace {

using Clock = Ticker::Clock;

Clock::time_point at(int milliseconds)
{
    return Clock::time_point{std::chrono::milliseconds{milliseconds}};
}

} // namespace

DUNGEON_TEST(core, ticker_emits_one_update_per_interval)
{
    Ticker ticker(std::chrono::milliseconds{10});
    ticker.reset(at(0));

    CHECK_EQ(ticker.advance(at(5)), 0U);
    CHECK_EQ(ticker.advance(at(10)), 1U);
    CHECK_EQ(ticker.advance(at(20)), 1U);
    CHECK_EQ(ticker.remainder(), Ticker::Duration::zero());
    CHECK_EQ(ticker.next_tick(), at(30));
}

DUNGEON_TEST(core, ticker_emits_multiple_updates_for_a_long_frame)
{
    Ticker ticker(std::chrono::milliseconds{10});
    ticker.reset(at(0));

    CHECK_EQ(ticker.advance(at(35)), 3U);
    CHECK_EQ(ticker.remainder(), std::chrono::milliseconds{5});
    CHECK_EQ(ticker.next_tick(), at(40));
}

DUNGEON_TEST(core, ticker_clamps_long_stalls)
{
    Ticker ticker(std::chrono::milliseconds{10}, std::chrono::milliseconds{25},
                  std::size_t{10});
    ticker.reset(at(0));

    CHECK_EQ(ticker.advance(at(1000)), 2U);
    CHECK_EQ(ticker.remainder(), std::chrono::milliseconds{5});
    CHECK_EQ(ticker.next_tick(), at(1005));
}

DUNGEON_TEST(core, ticker_caps_catch_up_work)
{
    Ticker ticker(std::chrono::milliseconds{10}, std::chrono::seconds{1},
                  std::size_t{3});
    ticker.reset(at(0));

    CHECK_EQ(ticker.advance(at(100)), 3U);
    CHECK_EQ(ticker.remainder(), Ticker::Duration::zero());
    CHECK_EQ(ticker.advance(at(110)), 1U);
}

DUNGEON_TEST(core, ticker_ignores_a_backwards_clock)
{
    Ticker ticker(std::chrono::milliseconds{10});
    ticker.reset(at(100));

    CHECK_EQ(ticker.advance(at(90)), 0U);
    CHECK_EQ(ticker.next_tick(), at(100));
    CHECK_EQ(ticker.advance(at(100)), 1U);
}

DUNGEON_TEST(core, ticker_rejects_invalid_configuration)
{
    CHECK_THROWS_AS(Ticker(std::chrono::milliseconds{0}), std::invalid_argument);
    CHECK_THROWS_AS(Ticker(std::chrono::milliseconds{10}, std::chrono::milliseconds{0}),
                    std::invalid_argument);
    CHECK_THROWS_AS(
        Ticker(std::chrono::milliseconds{10}, std::chrono::milliseconds{250}, std::size_t{0}),
        std::invalid_argument);
}
