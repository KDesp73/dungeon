#pragma once

#include "dungeon/map/chance.hpp"
#include "dungeon/map/entities.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <utility>
#include <vector>

namespace dungeon::map {

/// Shared handle to an entity stored by a `Map`.
using EntityPtr = std::shared_ptr<entities::Entity>;

/// Terrain occupying a single map cell.
enum class Tile : unsigned char {
    Empty,
    HWall,
    VWall,
    Walkway,
    Floor,
    Door,
};

/// ASCII glyph used to display a tile.
[[nodiscard]] char glyph(Tile tile) noexcept;

/// Side of a room where a door can be placed.
enum class DoorSide : unsigned char {
    Top,
    Right,
    Bottom,
    Left,
};

/// A cell coordinate inside a `MapSpec`.
struct Point {
    std::size_t x = 0;
    std::size_t y = 0;
};

/// Position and size of one room inside a `MapSpec`.
///
/// `width` and `height` include the walls, so a room of `6 x 5` has a
/// `4 x 3` interior.
struct RoomSpec {
    std::size_t x      = 0;
    std::size_t y      = 0;
    std::size_t width  = 0;
    std::size_t height = 0;
};

/// A walkway connecting two rooms in a `MapSpec`.
///
/// `from` and `to` index into `MapSpec::rooms`. The door on each room is
/// placed in the middle of `from_side` / `to_side`.
struct ConnectionSpec {
    std::size_t from      = 0;
    std::size_t to        = 0;
    DoorSide    from_side = DoorSide::Right;
    DoorSide    to_side   = DoorSide::Left;
};

/// One item the generator places inside a room.
///
/// `contents` is only used by chests and describes the loot placed inside.
struct ItemSpec {
    entities::ItemKind              kind = entities::ItemKind::Coin;
    std::size_t                     x    = 0;
    std::size_t                     y    = 0;
    std::vector<entities::ItemKind> contents;

    ItemSpec() = default;

    ItemSpec(entities::ItemKind item_kind, std::size_t item_x, std::size_t item_y)
        : kind(item_kind), x(item_x), y(item_y) {}

    ItemSpec(entities::ItemKind item_kind, std::size_t item_x, std::size_t item_y,
             std::vector<entities::ItemKind> item_contents)
        : kind(item_kind), x(item_x), y(item_y), contents(std::move(item_contents)) {}
};

/// One enemy the generator places inside a room.
struct EnemySpec {
    entities::EnemyKind kind = entities::EnemyKind::Fast;
    std::size_t         x    = 0;
    std::size_t         y    = 0;

    EnemySpec() = default;

    EnemySpec(entities::EnemyKind enemy_kind, std::size_t enemy_x, std::size_t enemy_y)
        : kind(enemy_kind), x(enemy_x), y(enemy_y) {}
};

/// Complete description of a generated dungeon.
///
/// Create one with `Map::make_spec()` or `Map::make_store_spec()` and pass it
/// to `Map::generate(const MapSpec&)` to rebuild the exact same map.
struct MapSpec {
    std::size_t                 width  = 0;
    std::size_t                 height = 0;
    std::vector<RoomSpec>       rooms;
    std::vector<ConnectionSpec> connections;
    Point                       entrance;
    Point                       exit;
    std::vector<ItemSpec>       items;
    std::vector<EnemySpec>      enemies;

    /// When true the exit works without the player carrying a key.
    bool exit_open = false;

    /// Places a shopkeeper at @p shopkeeper.
    bool  has_shopkeeper = false;
    Point shopkeeper;

    /// Places a decorative store counter at @p stand.
    bool  has_stand = false;
    Point stand;
};

inline const char32_t EXIT_GLYPH = '[';

/// A rectangular tile map.
///
/// Tiles are stored in a single contiguous buffer in row-major order, so a map
/// of `width()` columns and `height()` rows contains `width() * height()` tiles.
class Map {
  public:
    Map() = default;

    /// Creates a map that generates reproducible dungeons from @p seed.
    explicit Map(std::uint32_t seed) : _chance(seed) {}

    /// Replaces the generator seed and restarts the random sequence.
    void reseed(std::uint32_t seed) noexcept { _chance.reseed(seed); }

    /// Builds a random dungeon.
    ///
    /// The number, positions, and dimensions of the rooms vary on every call.
    /// If the requested dimensions are too small to contain a valid dungeon,
    /// the map is left as open background. The map never exceeds @p width by
    /// @p height.
    void generate(std::size_t width = 64, std::size_t height = 32);

    /// Builds the dungeon described by @p spec.
    ///
    /// The map is cleared first. Passing a spec produced by `make_spec()`
    /// recreates that layout exactly.
    ///
    /// @throws std::invalid_argument when a room is smaller than 3x3.
    /// @throws std::out_of_range when a room does not fit inside the map or a
    ///         connection references a room that does not exist.
    void generate(const MapSpec& spec);

    /// Creates a random dungeon layout without changing the map.
    ///
    /// Rooms are placed on an optional 3x3 grid, with slight random variation
    /// in their size and position, and connected by horizontal or vertical
    /// walkways. The spec includes an entrance, a distant exit, and the items
    /// rolled for each room. Pass the result to `generate(const MapSpec&)` to
    /// build it.
    [[nodiscard]] MapSpec make_spec(std::size_t width = 64, std::size_t height = 32);

    /// Creates the layout of a store level.
    ///
    /// A store is a single room with an entrance at the bottom, an already open
    /// exit at the top, and a shopkeeper with a counter in the middle. It has no
    /// enemies and no dungeon key. The returned spec is empty when @p width or
    /// @p height is too small to hold a store.
    [[nodiscard]] MapSpec make_store_spec(std::size_t width = 64, std::size_t height = 32);

    /// Removes every tile and resets the map to zero width and height.
    void clear();

    [[nodiscard]] bool is_wall(std::size_t index);
    [[nodiscard]] bool is_wall(std::size_t x, std::size_t y);

    [[nodiscard]] std::size_t width() const noexcept { return _width; }
    [[nodiscard]] std::size_t height() const noexcept { return _height; }

    /// Floor cell where the player starts, or `{0, 0}` when no entrance exists.
    [[nodiscard]] Point entrance() const noexcept { return _entrance; }

    /// Floor cell that ends the dungeon, or `{0, 0}` when no exit exists.
    [[nodiscard]] Point exit() const noexcept { return _exit; }

    /// True when the exit can be used.
    ///
    /// Store exits are always open; other exits require the player to carry a
    /// key.
    [[nodiscard]] bool exit_unlocked() const noexcept;

    /// Seed used to initialise this map's random generator.
    ///
    /// Printing this value lets a run be replayed once seeds can be supplied
    /// to the generator again.
    [[nodiscard]] std::uint32_t seed() const noexcept { return _chance.seed(); }

    [[nodiscard]] bool empty() const noexcept { return _tiles.empty(); }

    /// Returns the tile at (@p x, @p y).
    ///
    /// @throws std::out_of_range when the coordinates are outside the map.
    [[nodiscard]] Tile at(std::size_t x, std::size_t y) const;

    /// Sets the tile at (@p x, @p y).
    ///
    /// @throws std::out_of_range when the coordinates are outside the map.
    void set(std::size_t x, std::size_t y, Tile tile);

    /// Returns true when the player can stand on (@p x, @p y).
    ///
    /// Floors, walkways, and doors are walkable. Walls and empty space are not,
    /// and neither are coordinates outside the map.
    [[nodiscard]] bool walkable(std::size_t x, std::size_t y) const noexcept;

    /// Writes the map as ASCII rows to @p fd.
    void print(FILE* fd = stdout) const;

    /// Adds @p entity to the map and returns its id.
    ///
    /// The map keeps ownership of the entity. Adding a `Player` also makes it
    /// available through `player()`.
    int add(EntityPtr entity);

    /// Removes the entity with @p id and returns whether it was found.
    bool remove(int id);

    /// Returns the player placed by the generator, or `nullptr` when there is
    /// no player. The pointer stays valid until the player is removed or the
    /// map is cleared or regenerated.
    [[nodiscard]] entities::Player*       player() noexcept { return _player; }
    [[nodiscard]] const entities::Player* player() const noexcept { return _player; }

    /// Returns the live entities, preserving their concrete types.
    [[nodiscard]] const std::vector<EntityPtr>& entity_ptrs() const noexcept { return _entities; }

    /// Returns value copies of the entities.
    ///
    /// Prefer `entity_ptrs()` when the concrete entity type matters: copying
    /// through `Entity` slices derived state.
    [[nodiscard]] std::vector<entities::Entity> entities() const;

  private:
    std::size_t       _width  = 0;
    std::size_t       _height = 0;
    Point             _entrance;
    Point             _exit;
    bool              _has_exit    = false;
    bool              _exit_open   = false;
    std::vector<Tile> _tiles;
    chance::Chance    _chance;

    std::vector<EntityPtr> _entities;
    entities::Player*      _player = nullptr;

    struct Door {
        std::size_t x;
        std::size_t y;
    };

    /// Validates that a room rectangle fits inside the map.
    ///
    /// @throws std::invalid_argument when the room is smaller than 3x3.
    /// @throws std::out_of_range when the room does not fit inside the map.
    void validate_room(std::size_t x, std::size_t y, std::size_t width, std::size_t height) const;

    /// Draws a rectangular room whose top-left wall is at (@p x, @p y).
    ///
    /// @p width and @p height include the walls, so the interior is
    /// `(width - 2) x (height - 2)` tiles and is filled with `Tile::Floor`.
    /// Doors are added separately with `door()`.
    ///
    /// @throws std::invalid_argument when the room is smaller than 3x3.
    /// @throws std::out_of_range when the room does not fit inside the map.
    void room(std::size_t x, std::size_t y, std::size_t width, std::size_t height);

    /// Places a `Tile::Door` in the middle of @p side and returns its position.
    ///
    /// @throws std::invalid_argument when the room is smaller than 3x3.
    /// @throws std::out_of_range when the room does not fit inside the map.
    [[nodiscard]] Door door(std::size_t x, std::size_t y, std::size_t width, std::size_t height,
                            DoorSide side);

    /// Carves a walkway between two cells.
    ///
    /// The route only uses horizontal and vertical steps, so it never produces
    /// diagonal corridors. Blocked rooms are routed around using the open space
    /// between the grid cells.
    ///
    /// @throws std::out_of_range when either endpoint is outside the map.
    /// @throws std::runtime_error when no route between the endpoints exists.
    void walkway(std::size_t start_x, std::size_t start_y, std::size_t end_x, std::size_t end_y,
                 const std::vector<RoomSpec>& rooms);
};

} // namespace dungeon::map
