#pragma once

#include "dungeon/controls.hpp"
#include "dungeon/map/chance.hpp"
#include "dungeon/map/map.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dungeon::game {

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
    /// attack cooldown is ready. Calling this after the player has died does
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

    /// Replaces the action hook. Passing an empty function disables it.
    void set_action_hook(ActionHook hook) { _action_hook = std::move(hook); }

    [[nodiscard]] map::Map&       map() noexcept { return _map; }
    [[nodiscard]] const map::Map& map() const noexcept { return _map; }

    /// The player entity, or `nullptr` when the map has no player.
    [[nodiscard]] map::entities::Player*       player() noexcept { return _map.player(); }
    [[nodiscard]] const map::entities::Player* player() const noexcept { return _map.player(); }

    /// Messages produced by gameplay, oldest first.
    [[nodiscard]] const std::vector<std::string>& messages() const noexcept { return _messages; }

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
    /// Coins and diamonds are added to the player's gold, while keys and health
    /// potions are equipped into the inventory; picked-up items are removed from
    /// the map. Chests have no default step effect.
    void apply_step_action(const ActionEvent& event);

    /// Generates the next dungeon, consumes the key, and carries the player's
    /// stats and remaining inventory into the new map.
    void advance_level();

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

    /// Appends @p message to the message log, dropping the oldest entry when
    /// the log is full.
    void push_message(std::string message);

    map::Map                 _map;
    ActionHook               _action_hook;
    std::vector<std::string> _messages;
    map::chance::Chance      _chance;
    std::size_t              _chest_selection = 0;
    bool                     _game_over       = false;
};

} // namespace dungeon::game
