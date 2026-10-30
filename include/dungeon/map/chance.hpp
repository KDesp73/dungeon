#pragma once

#include <cstdint>
#include <random>
namespace dungeon::map::chance {

const double COIN = 0.6;
const double COIN_DOUBLE = 0.3;
const double CHEST = 0.2;
const double ROOM_BIG = 0.35;

class Chance {
public:
    Chance() = default;
    explicit Chance(std::uint32_t seed) : _seed(seed), _rng(_seed) {}

    [[nodiscard]] std::uint32_t seed() const { return _seed; }

    // Returns true if a random roll succeeds based on probability (0.0 to 1.0)
    [[nodiscard]] bool roll(double probability) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(_rng) <= probability;
    }

    // Returns a random integer within [min, max]
    [[nodiscard]] std::size_t range(std::size_t min, std::size_t max) {
        std::uniform_int_distribution<std::size_t> dist(min, max);
        return dist(_rng);
    }

private:
    std::uint32_t _seed = static_cast<std::uint32_t>(std::random_device{}());
    std::mt19937 _rng{_seed};
};

}
