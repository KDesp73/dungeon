#pragma once

#include "dungeon/game.hpp"
#include "dungeon/map/map.hpp"
#include "dungeon/scene/scene_manager.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace dungeon::agent {

/// Which kind of scene currently owns input.
enum class SceneKind : unsigned char {
    Title,
    Game,
    Store,
    Pause,
    Other,
};

[[nodiscard]] const char* scene_name(SceneKind kind) noexcept;

/// Immutable snapshot of everything the agent is allowed to look at.
///
/// The full terrain grid is kept so the deterministic policy can path-find;
/// `visible` adds entity glyphs and is what gets sent to Kev.
struct AgentObservation {
    using Cell = map::Point;

    struct EnemyInfo {
        std::string    name;
        int            hp         = 0;
        int            attack     = 0;
        int            defence    = 0;
        std::size_t    distance   = 0;
        Cell           cell;
        bool           is_boss    = false;
        std::size_t    anchors    = 0;
    };

    struct Obstacle {
        std::size_t x     = 0;
        std::size_t y     = 0;
        char        glyph = '\0';
    };

    /// A collectible item sitting on the map. Coins and diamonds carry gold;
    /// equipment and potions are valuable without a direct gold value.
    struct LootInfo {
        std::string name;
        char        glyph = '\0';
        int         gold  = 0;
        Cell        cell;
        std::size_t distance = 0;
        bool        collectable = true; ///< False when the inventory cannot hold it.
    };

    struct StoreOption {
        std::string name;
        std::string description;
        int         price     = 0;
        bool        sold      = false;
        bool        affordable = false;
    };

    std::size_t depth            = 0;
    bool        store_level      = false;
    bool        boss_level       = false;
    bool        game_over        = false;
    bool        victory          = false;
    bool        store_open       = false;
    SceneKind   scene            = SceneKind::Other;
    std::size_t store_selection  = 0;

    int         hp      = 0;
    int         max_hp  = 0;
    int         gold    = 0;
    int         attack  = 0;
    int         defence = 0;
    int         stamina = 0;
    std::size_t potions = 0;
    bool        has_key = false;

    std::size_t anchors_remaining = 0;
    Cell        player;
    Cell        exit_cell;
    bool        exit_unlocked     = false;
    bool        key_on_map        = false;
    Cell        key_cell;
    std::size_t distance_to_key   = 0;

    std::optional<EnemyInfo> nearest_enemy;
    std::vector<Obstacle>    obstacles;
    std::vector<LootInfo>    loot; ///< Items on the map, excluding the quest key.

    bool                     chest_nearby    = false;
    std::vector<std::string> chest_items;
    std::size_t              chest_selection = 0;
    bool                     shopkeeper_nearby = false;

    std::vector<std::string> tiles;   ///< Terrain glyphs, one string per row.
    std::vector<std::string> visible; ///< Terrain plus entity glyphs.

    std::vector<StoreOption> store_items;
    std::vector<std::string> recent_messages;
    game::RunStats           run_stats;

    /// Builds a snapshot. Never keeps pointers into the game or scenes.
    [[nodiscard]] static AgentObservation from(const game::Game& game,
                                               const scene::SceneManager& scenes);

    /// Stable, compact JSON in the shape of Kev's `state` field.
    [[nodiscard]] std::string to_json() const;
};

/// A key within this Manhattan distance is worth grabbing before optional loot:
/// it unlocks the exit at very little detour cost.
inline constexpr std::size_t kNearKeyDistance = 6;

/// Manhattan distance between two cells.
[[nodiscard]] std::size_t manhattan(AgentObservation::Cell lhs,
                                    AgentObservation::Cell rhs) noexcept;

/// True when the cell is inside the observation's terrain grid.
[[nodiscard]] bool in_bounds(const AgentObservation& observation, std::size_t x,
                             std::size_t y) noexcept;

/// True when the cell can be stood on according to the observed terrain.
[[nodiscard]] bool walkable(const AgentObservation& observation, std::size_t x,
                            std::size_t y) noexcept;

} // namespace dungeon::agent
