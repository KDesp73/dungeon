#pragma once

#include "dungeon/display/style.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace dungeon::map::entities {

using Color = dungeon::display::Color;
using Style = dungeon::display::TextStyle;

class Entity {
    inline static int entity_id = 1;

  public:
    std::size_t x = 0;
    std::size_t y = 0;

    Entity(char tile, Color color = Color::White, Style style = Style::None, std::size_t x = 0,
           std::size_t y = 0)
        : x(x), y(y), _tile(tile), _color(color), _style(style), _id(entity_id++) {}

    virtual ~Entity() = default; // Good practice for polymorphic base classes

    [[nodiscard]] int   id() const { return _id; }
    [[nodiscard]] char  tile() const { return _tile; }
    [[nodiscard]] Color color() const { return _color; }
    [[nodiscard]] Style style() const { return _style; }
    void                set_tile(char tile) noexcept { _tile = tile; }
    void                hide() { _visible = false; }
    void                show() { _visible = true; }
    void                place(std::size_t xx, std::size_t yy);

  private:
    char  _tile;
    Color _color;
    Style _style;
    bool  _visible = true;
    int   _id;
};

/// Base maximum health before stamina bonuses are applied.
inline constexpr int PLAYER_MAX_HP = 100;

/// Stamina the player starts with. Only stamina above this value adds health.
inline constexpr int PLAYER_BASE_STAMINA = 10;

/// Number of item slots the player carries. Equipment that is already owned
/// can still be upgraded when every slot is full.
inline constexpr std::size_t PLAYER_INVENTORY_SLOTS = 10;

/// Extra maximum health granted by each point of stamina above the base.
inline constexpr int STAMINA_HP_BONUS = 2;

/// Health restored by a health potion, rolled in this inclusive range.
inline constexpr int HEALTH_POTION_HEAL_MIN = 15;
inline constexpr int HEALTH_POTION_HEAL_MAX = 30;

/// Stat gains applied whenever the player gains a character level.
inline constexpr int LEVEL_UP_ATTACK_BONUS  = 2;
inline constexpr int LEVEL_UP_DEFENCE_BONUS = 1;
inline constexpr int LEVEL_UP_STAMINA_BONUS = 2;

struct Stats {
    std::size_t level   = 1;
    int         hp      = PLAYER_MAX_HP;
    int         attack  = 10;
    int         defence = 5;
    int         stamina = 10;
    int         gold    = 0;
};

/// Maximum health for @p stats.
///
/// Every point of stamina above `PLAYER_BASE_STAMINA` raises the cap, so
/// equipment and level-ups that grant stamina make the player tougher as well
/// as increasing the amount a potion can restore.
[[nodiscard]] constexpr int max_hp(const Stats& stats) noexcept {
    const int bonus = stats.stamina > PLAYER_BASE_STAMINA
                          ? (stats.stamina - PLAYER_BASE_STAMINA) * STAMINA_HP_BONUS
                          : 0;
    return PLAYER_MAX_HP + bonus;
}

/// Builds a zero-based stats delta for an item.
///
/// `Stats` carries player defaults, so brace-initialising only one field (for
/// example `Stats{.gold = 1}`) would keep the default attack, defence, and
/// stamina values. Item bonuses must start from zero and set only their own
/// fields.
[[nodiscard]] constexpr Stats stat_delta(int attack = 0, int defence = 0, int stamina = 0,
                                         int gold = 0) noexcept {
    return Stats{.level   = 0,
                 .hp      = 0,
                 .attack  = attack,
                 .defence = defence,
                 .stamina = stamina,
                 .gold    = gold};
}

class Alive {
  public:
    Alive(Stats stats) : _stats(stats) {}
    Alive(int hp, int attack, int defence, int stamina, int gold = 0)
        : _stats{1, hp, attack, defence, stamina, gold} {}

    [[nodiscard]] Stats  stats() const { return _stats; }
    [[nodiscard]] Stats& stats() noexcept { return _stats; }
    [[nodiscard]] bool   alive() const { return _stats.hp > 0; }
    std::size_t          level_up() { return ++_stats.level; }

  protected:
    Stats _stats;
};

/// The enemy archetypes in the dungeon.
///
/// The archetypes trade speed against durability and damage: fast enemies are
/// fragile and hit weakly, while slow enemies are tough and hit hard.
enum class EnemyKind : unsigned char {
    Fast,
    Normal,
    Brute,
    Boss,
};

/// Fixed stats and behaviour shared by every enemy of one kind.
struct EnemyProfile {
    std::string_view name;
    char             glyph;
    Color            color;
    int              max_hp;
    int              attack;
    int              defence;
    int              gold_min;
    int              gold_max;
    double           move_interval;
    double           attack_interval;
    int              aggro_radius;
    std::string_view speed_label;
};

/// Fixed stats for every enemy archetype, indexed by `EnemyKind`.
///
/// These are the base values for the first dungeon. Deeper dungeons scale them
/// up with `Enemy::scale_for_depth()`, so the run keeps getting harder even as
/// the player buys upgrades.
inline constexpr EnemyProfile kEnemyProfiles[] = {
    // Ordered fastest to slowest. Bounty grows with the danger of the enemy.
    //Name   Glyph Color               HP   AT  DE   g   G  MI    ATI  AR   SL
    {"Rat",   'r', Color::BrightGreen,   8,   5,  2,   1,  2, 0.22, 0.9,  7, "Fast"},
    {"Goblin",'g', Color::Yellow      ,  24,  10,  6,   2,  4, 0.45, 1.1,  6, "Normal"},
    {"Ogre",  'O', Color::BrightRed  ,  60,  25,  4,   4,  8, 0.85, 1.4,  5, "Slow"},
    // Bosses never move or melee; their fireballs are driven by Boss instead.
    {"Boss",  'B', Color::BrightMagenta, 90,  20,  6,  25, 40, 1.00, 2.0, 99, "Static"},
};

/// Enemy stat gains per dungeon below the first.
///
/// Health climbs faster than attack so fights get longer and more dangerous as
/// the player descends, while the player's defence upgrades still keep damage
/// manageable. Bounty grows slowly on purpose: combined with the rising enemy
/// count it already produces a steady gold curve, and a faster curve would let
/// the player buy every store upgrade without making choices.
inline constexpr int ENEMY_HP_PER_DEPTH      = 4;
inline constexpr int ENEMY_ATTACK_PER_DEPTH  = 1;
inline constexpr int ENEMY_DEFENCE_PER_DEPTH = 3; // One defence point per three depths.
inline constexpr int ENEMY_GOLD_PER_DEPTH    = 12; // One extra coin per twelve depths.

/// Boss stats at tier one, before the per-tier bonus is applied.
inline constexpr int BOSS_BASE_HP      = 90;
inline constexpr int BOSS_BASE_ATTACK  = 20;
inline constexpr int BOSS_BASE_DEFENCE = 6;

/// Stat gains for every boss tier after the first.
inline constexpr int BOSS_HP_PER_TIER      = 70;
inline constexpr int BOSS_ATTACK_PER_TIER  = 6;
inline constexpr int BOSS_DEFENCE_PER_TIER = 3;

/// Extra stats granted to the final boss on top of its tier.
inline constexpr int FINAL_BOSS_HP_BONUS     = 150;
inline constexpr int FINAL_BOSS_ATTACK_BONUS = 8;

/// Delay between fireball volleys. Each tier makes the boss faster, down to the
/// minimum; the final boss fires slightly faster than an ordinary boss.
inline constexpr double BOSS_FIREBALL_INTERVAL_BASE = 2.1;
inline constexpr double BOSS_FIREBALL_INTERVAL_STEP = 0.25;
inline constexpr double BOSS_FIREBALL_INTERVAL_MIN  = 0.75;
inline constexpr double FINAL_BOSS_FIREBALL_BONUS   = 0.2;

/// How far a fireball travels before burning out.
inline constexpr std::size_t FIREBALL_RANGE_BASE      = 12;
inline constexpr std::size_t FIREBALL_RANGE_PER_TIER  = 2;

/// Seconds between fireball cell steps, and the boss-only speed-up per tier.
inline constexpr double FIREBALL_STEP_INTERVAL      = 0.18;
inline constexpr double BOSS_FIREBALL_STEP_PER_TIER = 0.02;
inline constexpr double BOSS_FIREBALL_STEP_MIN      = 0.10;

/// First tier whose boss can fire a rotating spiral.
inline constexpr std::size_t BOSS_SPIRAL_MIN_TIER = 3;

/// Maximum number of adjacent directions fired in one spiral volley.
inline constexpr std::size_t BOSS_SPIRAL_MAX_ARMS = 4;

/// Directions fired in one aimed burst.
inline constexpr std::size_t BOSS_AIMED_ARMS = 3;

/// Clockwise steps the spiral advances per volley. The faster step keeps the
/// pattern moving instead of letting the player walk between slow arms.
inline constexpr std::size_t BOSS_SPIRAL_ROTATION_STEP       = 2;
inline constexpr std::size_t BOSS_FAST_SPIRAL_ROTATION_STEP  = 3;
/// Tier where the spiral speeds up and aimed bursts become available.
inline constexpr std::size_t BOSS_ADVANCED_MIN_TIER = 4;

/// Attack patterns a boss can choose between.
enum class BossAttack : unsigned char {
    Ring,   ///< All directions at once.
    Spiral, ///< A rotating fan that sweeps around the arena.
    Aimed,  ///< A tight fan fired toward the player's current position.
};

/// Relative chance of each `BossAttack`, indexed by the enum value.
///
/// Early bosses keep the readable ring. Later bosses mix in the spiral, and the
/// final boss adds the aimed burst to keep the fight from becoming predictable.
[[nodiscard]] constexpr std::array<double, 3> boss_attack_weights(std::size_t tier,
                                                                  bool final_boss) noexcept {
    if (tier < BOSS_SPIRAL_MIN_TIER) {
        return {1.0, 0.0, 0.0};
    }
    if (final_boss) {
        return {0.15, 0.40, 0.45};
    }
    if (tier >= BOSS_ADVANCED_MIN_TIER) {
        return {0.20, 0.50, 0.30};
    }
    return {0.35, 0.65, 0.0};
}

/// How often a boss "giggles", and for how long the animation plays.
inline constexpr double BOSS_GIGGLE_INTERVAL = 6.0;
inline constexpr double BOSS_GIGGLE_DURATION = 0.35;

/// Number of anchors protecting a boss at @p tier.
///
/// The first boss is a straightforward duel. The second introduces two anchors
/// on opposite corners, and every later boss uses all four. The final boss also
/// has four, but faster fireballs and much higher stats.
[[nodiscard]] constexpr std::size_t boss_anchor_count(std::size_t tier) noexcept {
    if (tier <= 1) {
        return 0;
    }
    return tier == 2 ? 2 : 4;
}

/// Builds the stats for a boss of @p tier.
[[nodiscard]] constexpr Stats boss_stats(std::size_t tier, bool final_boss) noexcept {
    const int steps = tier == 0 ? 1 : static_cast<int>(tier);
    return Stats{
        .level   = 1,
        .hp      = BOSS_BASE_HP + steps * BOSS_HP_PER_TIER + (final_boss ? FINAL_BOSS_HP_BONUS : 0),
        .attack  = BOSS_BASE_ATTACK + steps * BOSS_ATTACK_PER_TIER +
                   (final_boss ? FINAL_BOSS_ATTACK_BONUS : 0),
        .defence = BOSS_BASE_DEFENCE + steps * BOSS_DEFENCE_PER_TIER,
        .stamina = 0,
        .gold    = 0,
    };
}

/// Returns the profile for @p kind.
[[nodiscard]] constexpr const EnemyProfile& enemy_profile(EnemyKind kind) noexcept {
    return kEnemyProfiles[static_cast<std::size_t>(kind)];
}

class Enemy : public Entity, public Alive {
  public:
    Enemy(EnemyKind kind, std::size_t x, std::size_t y)
        : Entity(enemy_profile(kind).glyph, enemy_profile(kind).color, Style::Bold, x, y),
          Alive(enemy_profile(kind).max_hp, enemy_profile(kind).attack,
                enemy_profile(kind).defence, 0),
          _kind(kind),
          _max_hp(enemy_profile(kind).max_hp) {}

    /// Creates a normal enemy. Kept for callers that do not care about the
    /// archetype.
    Enemy(std::size_t x, std::size_t y) : Enemy(EnemyKind::Normal, x, y) {}

    [[nodiscard]] EnemyKind                kind() const noexcept { return _kind; }
    [[nodiscard]] const EnemyProfile&      profile() const noexcept { return enemy_profile(_kind); }
    [[nodiscard]] std::string_view         name() const noexcept { return profile().name; }
    [[nodiscard]] int                      max_hp() const noexcept { return _max_hp; }
    [[nodiscard]] bool                     is_boss() const noexcept {
        return _kind == EnemyKind::Boss;
    }
    [[nodiscard]] std::string_view         speed_label() const noexcept {
        return profile().speed_label;
    }

    /// Replaces the combat stats after construction, used by the boss subclass.
    void set_stats(Stats stats) noexcept { _stats = stats; }
    void set_max_hp(int hp) noexcept { _max_hp = hp; }
    [[nodiscard]] int gold_min() const noexcept { return profile().gold_min + _gold_bonus; }
    [[nodiscard]] int gold_max() const noexcept { return profile().gold_max + _gold_bonus; }

    /// Raises the enemy's stats to match dungeon @p depth.
    ///
    /// The first dungeon keeps the profile values; every deeper dungeon adds
    /// health, attack, defence, and bounty. Calling this more than once would
    /// stack the bonus, so it is applied exactly once when an enemy is placed.
    void scale_for_depth(std::size_t depth) noexcept {
        if (depth <= 1) {
            return;
        }

        const auto steps      = static_cast<int>(depth - 1);
        const int  hp_bonus   = steps * ENEMY_HP_PER_DEPTH;
        const int  gold_bonus = steps / ENEMY_GOLD_PER_DEPTH;

        stats().hp += hp_bonus;
        _max_hp += hp_bonus;
        stats().attack += steps * ENEMY_ATTACK_PER_DEPTH;
        stats().defence += steps / ENEMY_DEFENCE_PER_DEPTH;
        _gold_bonus += gold_bonus;
    }

    /// Reduces the enemy's health, never below zero.
    void take_damage(int damage) noexcept {
        if (damage <= 0) {
            return;
        }

        stats().hp = stats().hp > damage ? stats().hp - damage : 0;
    }

    /// Adds @p seconds to the enemy's movement and attack clocks.
    void advance_timers(double seconds) noexcept {
        _move_elapsed += seconds;
        _attack_elapsed += seconds;
    }

    [[nodiscard]] bool move_ready() const noexcept {
        return _move_elapsed >= profile().move_interval;
    }

    [[nodiscard]] bool attack_ready() const noexcept {
        return _attack_elapsed >= profile().attack_interval;
    }

    void reset_move_timer() noexcept { _move_elapsed = 0.0; }
    void reset_attack_timer() noexcept { _attack_elapsed = 0.0; }

    /// True once the enemy has spotted the player and will chase.
    [[nodiscard]] bool alerted() const noexcept { return _alerted; }
    void               alert() noexcept { _alerted = true; }

    [[nodiscard]] int  wander_direction() const noexcept { return _wander_direction; }
    void set_wander_direction(int direction) noexcept { _wander_direction = direction; }

  private:
    EnemyKind _kind;
    int       _max_hp;
    int       _gold_bonus       = 0;
    double    _move_elapsed     = 0.0;
    double    _attack_elapsed   = 0.0;
    bool      _alerted          = false;
    int       _wander_direction = 0;
};

/// A static arena boss.
///
/// Bosses do not walk and do not melee. Instead they fire volleys of fireballs
/// in all directions, with diagonals from tier two onward. Later bosses choose
/// between a full ring, a rotating spiral, and an aimed burst. The spiral now
/// advances several steps per volley so it stays dangerous. They take no damage
/// while any anchor is still standing.
class Boss final : public Enemy {
  public:
    Boss(std::size_t x, std::size_t y, std::size_t tier = 1, bool final_boss = false)
        : Enemy(EnemyKind::Boss, x, y), _tier(tier == 0 ? 1 : tier), _final(final_boss),
          _anchors_remaining(boss_anchor_count(tier)) {
        const Stats stats = boss_stats(_tier, final_boss);
        set_stats(stats);
        set_max_hp(stats.hp);
    }

    [[nodiscard]] std::size_t tier() const noexcept { return _tier; }
    [[nodiscard]] bool        is_final() const noexcept { return _final; }

    /// True once every anchor has been destroyed.
    [[nodiscard]] bool vulnerable() const noexcept { return _anchors_remaining == 0; }
    [[nodiscard]] std::size_t anchors_remaining() const noexcept { return _anchors_remaining; }
    void set_anchors_remaining(std::size_t count) noexcept { _anchors_remaining = count; }

    /// Seconds between fireball volleys for this boss.
    [[nodiscard]] double fireball_interval() const noexcept {
        const int  steps   = static_cast<int>(_tier) - 1;
        double     interval = BOSS_FIREBALL_INTERVAL_BASE -
                              static_cast<double>(steps) * BOSS_FIREBALL_INTERVAL_STEP;
        if (_final) {
            interval -= FINAL_BOSS_FIREBALL_BONUS;
        }
        return interval > BOSS_FIREBALL_INTERVAL_MIN ? interval : BOSS_FIREBALL_INTERVAL_MIN;
    }

    /// How far this boss's fireballs travel.
    [[nodiscard]] std::size_t fireball_range() const noexcept {
        return FIREBALL_RANGE_BASE + (_tier - 1) * FIREBALL_RANGE_PER_TIER;
    }

    /// Cardinal directions for the first boss; diagonals join from tier two.
    [[nodiscard]] std::size_t fireball_directions() const noexcept {
        return _tier >= 2 ? 8U : 4U;
    }

    /// True when this boss is allowed to fire a rotating spiral.
    [[nodiscard]] bool spiral_fire() const noexcept {
        return _tier >= BOSS_SPIRAL_MIN_TIER;
    }

    /// Pattern used for the opening volley. It opens with its signature spiral
    /// before later volleys are randomized.
    [[nodiscard]] BossAttack opening_attack() const noexcept {
        return spiral_fire() ? BossAttack::Spiral : BossAttack::Ring;
    }

    /// Pattern selected for the current volley, and the number fired so far.
    [[nodiscard]] BossAttack attack_pattern() const noexcept { return _attack; }
    void set_attack_pattern(BossAttack pattern) noexcept { _attack = pattern; }
    [[nodiscard]] std::size_t volleys_fired() const noexcept { return _volleys_fired; }
    void record_volley() noexcept { ++_volleys_fired; }

    /// Directions fired in one spiral volley. The fan widens with each tier.
    [[nodiscard]] std::size_t volley_size() const noexcept {
        if (!spiral_fire()) {
            return fireball_directions();
        }
        const std::size_t arms = _tier - 1;
        return arms > BOSS_SPIRAL_MAX_ARMS ? BOSS_SPIRAL_MAX_ARMS : arms;
    }

    /// Seconds between fireball cell steps. Deeper bosses shoot faster, so
    /// their volleys are harder to walk out of.
    [[nodiscard]] double fireball_step_interval() const noexcept {
        const double saved = BOSS_FIREBALL_STEP_PER_TIER * static_cast<double>(_tier - 1);
        const double interval = FIREBALL_STEP_INTERVAL - saved;
        return interval > BOSS_FIREBALL_STEP_MIN ? interval : BOSS_FIREBALL_STEP_MIN;
    }

    /// Current clockwise offset of the spiral, and the step to the next volley.
    [[nodiscard]] std::size_t volley_rotation() const noexcept { return _volley_rotation; }
    [[nodiscard]] std::size_t volley_rotation_step() const noexcept {
        return _tier >= BOSS_ADVANCED_MIN_TIER ? BOSS_FAST_SPIRAL_ROTATION_STEP
                                               : BOSS_SPIRAL_ROTATION_STEP;
    }
    void advance_volley_rotation() noexcept {
        _volley_rotation = (_volley_rotation + volley_rotation_step()) % 8;
    }

    /// A short visual flicker used for the boss's "giggle".
    void set_giggling(bool giggling) noexcept { set_tile(giggling ? 'b' : 'B'); }

    [[nodiscard]] bool        fireball_ready() const noexcept {
        return _fireball_elapsed >= fireball_interval();
    }
    void advance_fireball_timer(double seconds) noexcept { _fireball_elapsed += seconds; }
    void reset_fireball_timer() noexcept { _fireball_elapsed = 0.0; }

  private:
    std::size_t _tier;
    bool        _final;
    std::size_t _anchors_remaining;
    double      _fireball_elapsed = 0.0;
    std::size_t _volley_rotation  = 0;
    std::size_t _volleys_fired    = 0;
    BossAttack  _attack           = BossAttack::Ring;
};

/// A destructible pillar protecting a boss.
///
/// Anchors are inert: the player breaks one by walking into it, just like
/// attacking an enemy. They are drawn as `A` and block fireballs.
class Anchor : public Entity {
  public:
    Anchor(std::size_t x, std::size_t y, int hits)
        : Entity('A', Color::BrightCyan, Style::Bold, x, y), _hits(hits > 1 ? hits : 1) {}

    void take_damage(int damage = 1) noexcept {
        if (damage <= 0) {
            return;
        }
        _hits = _hits > damage ? _hits - damage : 0;
    }

    [[nodiscard]] bool alive() const noexcept { return _hits > 0; }
    [[nodiscard]] int  hits() const noexcept { return _hits; }

  private:
    int _hits;
};

/// A fireball travelling in a straight line away from a boss.
class Fireball : public Entity {
  public:
    Fireball(std::size_t x, std::size_t y, int dx, int dy, int damage, std::size_t range,
             double step_interval = FIREBALL_STEP_INTERVAL)
        : Entity('*', Color::BrightRed, Style::Bold, x, y), _dx(dx), _dy(dy), _damage(damage),
          _range(range), _step_interval(step_interval) {}

    [[nodiscard]] int         dx() const noexcept { return _dx; }
    [[nodiscard]] int         dy() const noexcept { return _dy; }
    [[nodiscard]] int         damage() const noexcept { return _damage; }
    [[nodiscard]] bool        expired() const noexcept { return _range == 0; }
    [[nodiscard]] double      step_interval() const noexcept { return _step_interval; }

    void advance(double seconds) noexcept { _elapsed += seconds; }
    [[nodiscard]] bool step_ready() const noexcept { return _elapsed >= _step_interval; }
    void reset_step_timer() noexcept { _elapsed = 0.0; }
    void consume_step() noexcept {
        if (_range > 0) {
            --_range;
        }
    }

  private:
    int         _dx;
    int         _dy;
    int         _damage;
    std::size_t _range;
    double      _step_interval;
    double      _elapsed = 0.0;
};

/// The keeper of a store level.
///
/// The shop menu opens when the player comes close to this entity.
class Shopkeeper : public Entity {
  public:
    Shopkeeper(std::size_t x = 0, std::size_t y = 0)
        : Entity('&', Color::BrightYellow, Style::Bold, x, y) {}
};

/// The counter the shopkeeper stands behind.
///
/// It is decorative; the shop menu is triggered by the shopkeeper.
class StoreStand : public Entity {
  public:
    StoreStand(std::size_t x = 0, std::size_t y = 0)
        : Entity('=', Color::BrightBlack, Style::None, x, y) {}
};

enum class ItemType : unsigned char {
    Equipable,
    Interactable
};

/// Item kinds the dungeon generator can place in a room.
///
/// The equipment kinds are sold in store levels and can also be placed by an
/// explicit `MapSpec`.
enum class ItemKind : unsigned char {
    Coin,
    CoinDouble,
    Key,
    Chest,
    Diamond,
    HealthPotion,
    Sword,
    Greatsword,
    Shield,
    Chainmail,
    Boots,
};

/// Returns true when @p kind is a piece of equipment that can be upgraded.
[[nodiscard]] constexpr bool is_equipment(ItemKind kind) noexcept {
    switch (kind) {
    case ItemKind::Sword:
    case ItemKind::Greatsword:
    case ItemKind::Shield:
    case ItemKind::Chainmail:
    case ItemKind::Boots:
        return true;
    case ItemKind::Coin:
    case ItemKind::CoinDouble:
    case ItemKind::Key:
    case ItemKind::Chest:
    case ItemKind::Diamond:
    case ItemKind::HealthPotion:
        return false;
    }

    return false;
}

class Item : public Entity {
  public:
    Item(char tile, Color color, Style style, ItemType type, ItemKind kind, Stats stats,
         std::size_t x = 0, std::size_t y = 0)
        : Entity(tile, color, style, x, y),
          _base_stats(stats),
          _stats_diff(stats),
          _type(type),
          _kind(kind) {}

    Item(char tile, Color color, Style style, ItemType type, ItemKind kind, int attack, int defence,
         int stamina, int gold, std::size_t x = 0, std::size_t y = 0)
        : Entity(tile, color, style, x, y),
          _base_stats{0, 0, attack, defence, stamina, gold},
          _stats_diff(_base_stats),
          _type(type),
          _kind(kind) {}

    [[nodiscard]] bool     equipable() const { return _type == ItemType::Equipable; }
    [[nodiscard]] bool     interactable() const { return _type == ItemType::Interactable; }
    [[nodiscard]] ItemKind kind() const { return _kind; }
    [[nodiscard]] int      gold() const { return _stats_diff.gold; }
    [[nodiscard]] bool     upgradable() const noexcept { return is_equipment(_kind); }

    /// Current upgrade level. Fresh equipment starts at level one.
    [[nodiscard]] std::size_t level() const noexcept { return _level; }

    bool equip(Stats& target) {
        if (!equipable())
            return false;
        target.attack += _stats_diff.attack;
        target.defence += _stats_diff.defence;
        target.stamina += _stats_diff.stamina;
        target.gold += _stats_diff.gold;
        return true;
    }

    /// Raises the item by one level and applies the extra bonus to @p target.
    ///
    /// Returns false for items that cannot be upgraded, such as coins and
    /// potions.
    bool upgrade(Stats& target) {
        if (!upgradable())
            return false;

        ++_level;
        _stats_diff.attack += _base_stats.attack;
        _stats_diff.defence += _base_stats.defence;
        _stats_diff.stamina += _base_stats.stamina;
        _stats_diff.gold += _base_stats.gold;

        target.attack += _base_stats.attack;
        target.defence += _base_stats.defence;
        target.stamina += _base_stats.stamina;
        target.gold += _base_stats.gold;
        return true;
    }

    void drop(Stats& target) {
        target.attack -= _stats_diff.attack;
        target.defence -= _stats_diff.defence;
        target.stamina -= _stats_diff.stamina;
        target.gold -= _stats_diff.gold;
    }

  private:
    Stats       _base_stats;
    Stats       _stats_diff;
    ItemType    _type;
    ItemKind    _kind;
    std::size_t _level = 1;
};

class Coin : public Item {
  public:
    Coin(std::size_t x, std::size_t y)
        : Item('$', Color::Yellow, Style::Bold, ItemType::Equipable, ItemKind::Coin,
               stat_delta(0, 0, 0, 1), x, y) {}
};

class CoinDouble : public Item {
  public:
    CoinDouble(std::size_t xx, std::size_t yy)
        : Item('$', Color::Yellow, Style::Bold, ItemType::Equipable, ItemKind::CoinDouble,
               stat_delta(0, 0, 0, 2), xx, yy) {}
};

class Diamond : public Item {
  public:
    Diamond(std::size_t x, std::size_t y)
        : Item('V', Color::Cyan, Style::Bold, ItemType::Equipable, ItemKind::Diamond,
               stat_delta(0, 0, 0, 10), x, y) {}
};

/// A consumable that restores a random amount of health when used.
///
/// Potions are carried in the inventory until the player drinks one; they do
/// not affect the player's stats when picked up.
class HealthPotion : public Item {
  public:
    HealthPotion(std::size_t x, std::size_t y)
        : Item('!', Color::BrightMagenta, Style::Bold, ItemType::Equipable,
               ItemKind::HealthPotion, stat_delta(), x, y) {}
};

class Key : public Item {
  public:
    Key(std::size_t x, std::size_t y)
        : Item('k', Color::Cyan, Style::Bold, ItemType::Equipable, ItemKind::Key, stat_delta(), x,
               y) {}
};

/// Equipment sold in stores. Each kind raises the player's stats when equipped.
class Sword : public Item {
  public:
    Sword(std::size_t x = 0, std::size_t y = 0)
        : Item('/', Color::BrightCyan, Style::Bold, ItemType::Equipable, ItemKind::Sword,
               stat_delta(2), x, y) {}
};

class Greatsword : public Item {
  public:
    Greatsword(std::size_t x = 0, std::size_t y = 0)
        : Item('T', Color::BrightYellow, Style::Bold, ItemType::Equipable, ItemKind::Greatsword,
               stat_delta(5), x, y) {}
};

class Shield : public Item {
  public:
    Shield(std::size_t x = 0, std::size_t y = 0)
        : Item('d', Color::BrightBlue, Style::Bold, ItemType::Equipable, ItemKind::Shield,
               stat_delta(0, 2), x, y) {}
};

class Chainmail : public Item {
  public:
    Chainmail(std::size_t x = 0, std::size_t y = 0)
        : Item('m', Color::BrightCyan, Style::Bold, ItemType::Equipable, ItemKind::Chainmail,
               stat_delta(0, 3, 1), x, y) {}
};

class Boots : public Item {
  public:
    Boots(std::size_t x = 0, std::size_t y = 0)
        : Item('b', Color::Green, Style::Bold, ItemType::Equipable, ItemKind::Boots,
               stat_delta(0, 0, 2), x, y) {}
};

/// Returns a fresh item of @p kind placed at (@p x, @p y).
///
/// Chests are not supported because they carry their own contents; a coin is
/// returned instead.
[[nodiscard]] Item make_item(ItemKind kind, std::size_t x = 0, std::size_t y = 0);

class Chest : public Item {
  public:
    Chest(std::size_t x, std::size_t y)
        : Item('?', Color::Magenta, Style::Bold, ItemType::Interactable, ItemKind::Chest,
               stat_delta(), x, y) {}

    /// Items still inside the chest, in display order.
    [[nodiscard]] const std::vector<Item>& contents() const noexcept { return _contents; }
    [[nodiscard]] bool                     empty() const noexcept { return _contents.empty(); }

    /// Adds @p item to the chest.
    void add(Item item) { _contents.push_back(std::move(item)); }

    /// Removes and returns the item at @p index.
    ///
    /// Returns `std::nullopt` when @p index is out of range.
    [[nodiscard]] std::optional<Item> take(std::size_t index) {
        if (index >= _contents.size()) {
            return std::nullopt;
        }

        Item item = std::move(_contents[index]);
        _contents.erase(_contents.begin() + static_cast<std::ptrdiff_t>(index));
        return item;
    }

  private:
    std::vector<Item> _contents;
};

class Player : public Entity, public Alive {
  public:
    Player(std::size_t x, std::size_t y)
        : Entity('@', Color::White, Style::Bold, x, y),
          Alive(PLAYER_MAX_HP, 13, 5, PLAYER_BASE_STAMINA) {}

    [[nodiscard]] const std::vector<Item>& inventory() const noexcept { return _inventory; }
    [[nodiscard]] bool                     has_item(ItemKind kind) const;
    [[nodiscard]] bool                     has_key() const { return has_item(ItemKind::Key); }

    /// True when every inventory slot is occupied.
    [[nodiscard]] bool inventory_full() const noexcept {
        return _inventory.size() >= PLAYER_INVENTORY_SLOTS;
    }

    /// Returns true when @p kind can be stored without exceeding the slots.
    ///
    /// Already owned equipment is always accepted because it is upgraded in
    /// place rather than taking a second slot.
    [[nodiscard]] bool can_equip(ItemKind kind) const noexcept {
        return (is_equipment(kind) && has_item(kind)) || !inventory_full();
    }

    /// Replaces the player's stats and inventory.
    ///
    /// Used when carrying the player into a freshly generated dungeon.
    void restore(Stats new_stats, std::vector<Item> new_inventory) {
        stats()    = new_stats;
        _inventory = std::move(new_inventory);
    }

    /// Equips @p item.
    ///
    /// When a piece of equipment of the same kind is already carried, the
    /// existing item is upgraded instead of adding a duplicate. Returns false
    /// when the inventory has no free slot or the item cannot be equipped.
    [[nodiscard]] bool equip(Item item);
    void               drop(int id);
    void               drop(ItemKind kind);

  private:
    std::vector<Item> _inventory;
};

} // namespace dungeon::map::entities
