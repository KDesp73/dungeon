#pragma once

#include "dungeon/map/entities.hpp"

#include <cstdint>
#include <random>
#include <span>
namespace dungeon::map::chance {

/// Probability that a room contains a single coin.
inline constexpr double COIN = 0.6;
/// Probability that a room contains a double-value coin.
inline constexpr double COIN_DOUBLE = 0.3;
/// Probability that a room contains a chest.
inline constexpr double CHEST = 0.2;
/// Probability that a room uses the larger size range.
inline constexpr double ROOM_BIG = 0.35;

/// Relative weight for placing the key in the player's starting room.
inline constexpr double KEY_SAME_ROOM_WEIGHT = 0.1;
/// Relative weight for placing the key in a room connected to the entrance.
inline constexpr double KEY_ADJACENT_ROOM_WEIGHT = 0.35;
/// Relative weight for placing the key in any other room.
inline constexpr double KEY_DISTANT_ROOM_WEIGHT = 1.0;

/// One row of the room item spawn table.
struct ItemSpawn {
    entities::ItemKind kind;
    double             probability;
};

/// Items rolled independently for every room.
///
/// Add a row here to make an item appear in generated rooms. The matching
/// entity is created in `Map::generate(const MapSpec&)`. The key is excluded
/// because every dungeon places exactly one using the `KEY_*_WEIGHT` values.
inline constexpr ItemSpawn ITEM_SPAWNS[] = {
    {entities::ItemKind::Coin, COIN},
    {entities::ItemKind::CoinDouble, COIN_DOUBLE},
    {entities::ItemKind::Chest, CHEST},
};

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

    // Returns an index chosen with probability proportional to its weight.
    [[nodiscard]] std::size_t weighted(std::span<const double> weights) {
        if (weights.empty()) {
            return 0;
        }

        double total = 0.0;
        for (const double weight : weights) {
            total += weight;
        }
        if (total <= 0.0) {
            return 0;
        }

        std::uniform_real_distribution<double> dist(0.0, total);
        double pick = dist(_rng);

        for (std::size_t i = 0; i < weights.size(); ++i) {
            pick -= weights[i];
            if (pick <= 0.0) {
                return i;
            }
        }
        return weights.size() - 1;
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
