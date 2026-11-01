#pragma once

#include "dungeon/controls.hpp"
#include "dungeon/map/map.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

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
    /// Returns false when there is no player or the destination is not
    /// walkable. A successful move dispatches the action hooks.
    [[nodiscard]] bool move(Direction direction);

    /// Replaces the action hook. Passing an empty function disables it.
    void set_action_hook(ActionHook hook) { _action_hook = std::move(hook); }

    [[nodiscard]] map::Map&       map() noexcept { return _map; }
    [[nodiscard]] const map::Map& map() const noexcept { return _map; }

    /// The player entity, or `nullptr` when the map has no player.
    [[nodiscard]] map::entities::Player*       player() noexcept { return _map.player(); }
    [[nodiscard]] const map::entities::Player* player() const noexcept { return _map.player(); }

  private:
    /// Scans the map after a move and calls `_action_hook` for every item the
    /// player stepped on or is standing next to.
    void dispatch_actions();

    map::Map   _map;
    ActionHook _action_hook;
};

} // namespace dungeon::game
