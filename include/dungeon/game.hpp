#pragma once

#include "dungeon/controls.hpp"
#include "dungeon/map/chance.hpp"
#include "dungeon/map/map.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace dungeon::game {

/// Number of dungeons between store levels.
inline constexpr std::size_t kStoreInterval = 5;

/// Number of dungeons between character level increases.
inline constexpr std::size_t kLevelUpInterval = 5;

/// Manhattan distance within which the shop menu opens.
inline constexpr std::size_t kShopkeeperReach = 1;

/// Percentage of an upgrade's base price added for every store visited after
/// the first. The first store sells at the base price, and each later store
/// raises every price slightly so that upgrades do not become trivial.
inline constexpr int kStorePriceGrowthPercent = 15;

/// A cardinal direction the player can move in.
enum class Direction : unsigned char {
    Up,
    Down,
    Left,
    Right,
};

/// A grid offset in cells.
struct Offset {
    int x = 0;
    int y = 0;
};

/// Returns the offset applied by @p direction.
[[nodiscard]] Offset offset(Direction direction) noexcept;

/// Maps a WASD key to a direction.
///
/// Returns `std::nullopt` for any other key. Both lower and upper case are
/// accepted.
[[nodiscard]] std::optional<Direction> direction_from_key(char key) noexcept;

/// Maps a decoded key event to a direction.
///
/// Arrow keys map to their matching direction and character keys are handed to
/// the WASD overload above. Returns `std::nullopt` for every other key.
[[nodiscard]] std::optional<Direction> direction_from_key(const controls::KeyEvent& key) noexcept;

/// Why an action hook was called.
enum class ActionTrigger : unsigned char {
    /// The player moved onto the item.
    Step,
    /// The player is orthogonally one square away from an interactable item.
    Nearby,
};

/// The action passed to an `ActionHook`.
///
/// `item` and `player` stay valid for the duration of the hook call. A hook
/// may remove the item from the map; the item is kept alive until the hook
/// returns.
struct ActionEvent {
    ActionTrigger          trigger;
    map::entities::Item&   item;
    map::entities::Player& player;
};

/// Callback invoked when the player steps on an item or is next to an
/// interactable.
///
/// The hook is called synchronously after every completed move, once for each
/// item that triggered. A nearby interactable triggers on each move made while
/// it stays in range, so hooks that change state should be idempotent.
using ActionHook = std::function<void(const ActionEvent&)>;

/// One purchasable upgrade offered by a store level.
struct StoreItem {
    map::entities::ItemKind kind{};
    std::string             name;
    std::string             description;
    int                     price = 0;
};

/// Owns the map and the rules that move the player through it.
class Game {
  public:
    Game() = default;

    /// Creates a game with a generated dungeon of @p width by @p height.
    explicit Game(std::size_t width, std::size_t height);

    /// Creates a game from an explicit map layout.
    explicit Game(const map::MapSpec& spec);

    /// Builds a random dungeon and resets the player.
    void generate(std::size_t width = 64, std::size_t height = 32);

    /// Builds the dungeon described by @p spec and resets the player.
    void generate(const map::MapSpec& spec);

    /// Moves the player one cell in @p direction.
    ///
    /// Returns false when there is no player, the player is dead, or the
    /// destination is not walkable. Moving into a living enemy attacks it
    /// instead of moving. A successful move dispatches the action hooks.
    /// Stepping onto the unlocked exit generates a new dungeon and carries the
    /// player's stats and inventory over to it.
    [[nodiscard]] bool move(Direction direction);

    /// Advances enemy movement and attacks by @p seconds of simulation time.
    ///
    /// Enemies wander independently until the player enters their aggro radius,
    /// then chase him. An enemy adjacent to the player attacks whenever its
    /// attack cooldown is ready. Damage is reduced by the player's defence and
    /// is never lower than one. Calling this after the player has died does
    /// nothing.
    void update(double seconds);

    /// Starts a fresh run in a newly generated dungeon of the same size.
    void restart();

    /// True once the player's health has reached zero.
    [[nodiscard]] bool game_over() const noexcept { return _game_over; }

    /// The living enemy closest to the player, or `nullptr` when none remain.
    [[nodiscard]] map::entities::Enemy*       nearest_enemy() noexcept;
    [[nodiscard]] const map::entities::Enemy* nearest_enemy() const noexcept;

    /// Drinks a health potion from the inventory.
    ///
    /// Restores a random amount of health without exceeding the player's
    /// maximum. Returns false when no potion is carried, the player is already
    /// at full health, or the run is over. A potion is only consumed on a
    /// successful use.
    [[nodiscard]] bool use_health_potion();

    /// Descends to the next dungeon, carrying the player's stats and inventory.
    ///
    /// Enemy density and strength grow with depth: the first dungeon is sparsely
    /// populated, and every dungeon below it adds more enemies and gives each
    /// one extra health, attack, defence, and bounty. The player's level
    /// increases every `kLevelUpInterval` dungeons, raising attack, defence,
    /// and stamina and restoring health. Every `kStoreInterval` dungeons the
    /// next dungeon is a store level: a room with a shopkeeper and an already
    /// open exit. This is called automatically after stepping onto the unlocked
    /// exit.
    void descend();

    /// Number of dungeons visited in this run, starting at one.
    [[nodiscard]] std::size_t depth() const noexcept { return _depth; }

    /// True while the current dungeon is a store level.
    [[nodiscard]] bool store_level() const noexcept { return _store_level; }

    /// True while the player has the shop menu open.
    [[nodiscard]] bool store_open() const noexcept { return _store_open; }

    /// Opens the shop menu.
    ///
    /// `GameScene` calls this when the player walks close to a shopkeeper.
    void open_store() noexcept { _store_open = true; }

    /// Upgrades offered by the current store level.
    ///
    /// The catalog is rebuilt for every store visit. Prices start at their base
    /// values and rise by `kStorePriceGrowthPercent` on each later visit.
    [[nodiscard]] std::span<const StoreItem> store_items() const noexcept;

    /// True when the store item at @p index was already bought this visit.
    [[nodiscard]] bool store_item_sold(std::size_t index) const noexcept;

    /// Buys or upgrades the store item at @p index.
    ///
    /// The item's stats are applied to the player and the price is deducted
    /// from the player's gold. If the player already carries the same kind of
    /// equipment it is upgraded instead of being added a second time. Returns
    /// false when the shop menu is closed, the index is invalid, the item was
    /// already bought this visit, or the player cannot afford it.
    [[nodiscard]] bool buy_store_item(std::size_t index);

    /// Closes the shop menu without leaving the store level.
    void close_store() noexcept { _store_open = false; }

    /// Replaces the action hook. Passing an empty function disables it.
    void set_action_hook(ActionHook hook) { _action_hook = std::move(hook); }

    [[nodiscard]] map::Map&       map() noexcept { return _map; }
    [[nodiscard]] const map::Map& map() const noexcept { return _map; }

    /// The player entity, or `nullptr` when the map has no player.
    [[nodiscard]] map::entities::Player*       player() noexcept { return _map.player(); }
    [[nodiscard]] const map::entities::Player* player() const noexcept { return _map.player(); }

    /// Messages produced by gameplay, oldest first.
    [[nodiscard]] const std::vector<std::string>& messages() const noexcept { return _messages; }

    /// The shopkeeper within `kShopkeeperReach` cells of the player.
    ///
    /// Returns `nullptr` when the player is too far away or the map has no
    /// shopkeeper.
    [[nodiscard]] map::entities::Shopkeeper*       nearby_shopkeeper() noexcept;
    [[nodiscard]] const map::entities::Shopkeeper* nearby_shopkeeper() const noexcept;

    /// The chest the player is standing on or orthogonally next to.
    ///
    /// Returns `nullptr` when no chest is in range.
    [[nodiscard]] map::entities::Chest*       nearby_chest() noexcept;
    [[nodiscard]] const map::entities::Chest* nearby_chest() const noexcept;

    /// Index of the chest item currently selected.
    [[nodiscard]] std::size_t chest_selection() const noexcept { return _chest_selection; }

    /// Moves the chest selection forward or backward, wrapping around.
    ///
    /// Does nothing when there is no chest in range. A selection of zero is
    /// kept while the chest is empty.
    void cycle_chest_selection(int delta) noexcept;

    /// Selects the chest item at @p index, clamped to the available items.
    void select_chest_item(std::size_t index) noexcept;

    /// Moves the selected chest item into the player's inventory or purse.
    ///
    /// Coins and diamonds add gold directly; other items are equipped by the
    /// player. Returns false when no chest is in range or the chest is empty.
    [[nodiscard]] bool take_selected_chest_item();

  private:
    /// Applies the built-in effect of stepping on @p event's item.
    ///
    /// Coins and diamonds are added to the player's gold, while keys, potions,
    /// and equipment are equipped into the inventory; picked-up items are
    /// removed from the map. Chests have no default step effect.
    void apply_step_action(const ActionEvent& event);

    /// Scans the map after a move and calls `_action_hook` for every item the
    /// player stepped on or is standing next to.
    void dispatch_actions();

    /// Attacks @p enemy with the player's current attack and removes it when
    /// its health reaches zero. Dead enemies drop a randomized pile of coins.
    void attack_enemy(map::entities::Enemy& enemy);

    /// Makes @p enemy attack the player when it is orthogonally adjacent.
    ///
    /// Damage is the enemy's attack reduced by the player's defence, with a
    /// minimum of one. Sets `game_over()` when the player's health reaches zero.
    void attack_player(map::entities::Enemy& enemy);

    /// Moves @p enemy one step toward the player using a breadth-first search
    /// around walls and other enemies.
    void chase_player(map::entities::Enemy& enemy);

    /// Moves @p enemy in a deterministic wandering pattern when it has not
    /// spotted the player.
    void wander(map::entities::Enemy& enemy);

    /// Returns the walkable first step on a path from (@p start_x, @p start_y)
    /// to (@p goal_x, @p goal_y), or `std::nullopt` when no path exists.
    [[nodiscard]] std::optional<Offset> first_step_toward(std::size_t start_x,
                                                          std::size_t start_y,
                                                          std::size_t goal_x,
                                                          std::size_t goal_y,
                                                          const map::entities::Enemy& mover) const;

    /// Returns the living enemy standing on (@p x, @p y), or `nullptr`.
    [[nodiscard]] map::entities::Enemy* enemy_at(std::size_t x, std::size_t y) noexcept;

    /// Returns true when another enemy or the player occupies (@p x, @p y).
    [[nodiscard]] bool occupied(std::size_t x, std::size_t y,
                                const map::entities::Enemy* ignore = nullptr) const noexcept;

    /// Applies the current dungeon depth to every enemy on the map.
    ///
    /// Enemies are created with their base profile stats, so this is called
    /// once after each new dungeon is generated.
    void scale_enemies_for_depth();

    /// Appends @p message to the message log, dropping the oldest entry when
    /// the log is full.
    void push_message(std::string message);

    map::Map                 _map;
    ActionHook               _action_hook;
    std::vector<std::string> _messages;
    map::chance::Chance      _chance;
    std::vector<StoreItem>   _store_items;
    std::vector<bool>        _store_sold;
    std::size_t              _chest_selection = 0;
    std::size_t              _depth           = 1;
    std::size_t              _store_visits    = 0;
    bool                     _store_level     = false;
    bool                     _store_open      = false;
    bool                     _game_over       = false;
};

} // namespace dungeon::game
