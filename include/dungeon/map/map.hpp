#pragma once

#include "dungeon/map/chance.hpp"
#include "dungeon/map/entities.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

namespace dungeon::map {

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

/// Complete description of a generated dungeon.
///
/// Create one with `Map::make_spec()` and pass it to
/// `Map::generate(const MapSpec&)` to rebuild the exact same map.
struct MapSpec {
    std::size_t                width  = 0;
    std::size_t                height = 0;
    std::vector<RoomSpec>       rooms;
    std::vector<ConnectionSpec> connections;
};

/// A rectangular tile map.
///
/// Tiles are stored in a single contiguous buffer in row-major order, so a map
/// of `width()` columns and `height()` rows contains `width() * height()` tiles.
class Map {
public:
    Map() = default;

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
    /// Pass the result to `generate(const MapSpec&)` to build it.
    [[nodiscard]] MapSpec make_spec(std::size_t width = 64, std::size_t height = 32);

    /// Removes every tile and resets the map to zero width and height.
    void clear();

    [[nodiscard]] bool is_wall(std::size_t index);
    [[nodiscard]] bool is_wall(std::size_t x, std::size_t y);

    [[nodiscard]] std::size_t width() const noexcept { return _width; }
    [[nodiscard]] std::size_t height() const noexcept { return _height; }

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

    /// Writes the map as ASCII rows to @p fd.
    void print(FILE* fd = stdout) const;

    int add(entities::Entity e);
    bool remove(int id);
    [[nodiscard]] std::vector<entities::Entity> entities() const { return _entities; }

private:
    std::size_t _width    = 0;
    std::size_t _height   = 0;
    std::size_t _entrance = 0;
    std::size_t _exit     = 0;
    std::vector<Tile> _tiles;
    chance::Chance _chance;

    std::vector<entities::Entity> _entities;

    struct Door {
        std::size_t x;
        std::size_t y;
    };

    /// Validates that a room rectangle fits inside the map.
    ///
    /// @throws std::invalid_argument when the room is smaller than 3x3.
    /// @throws std::out_of_range when the room does not fit inside the map.
    void validate_room(std::size_t x, std::size_t y, std::size_t width,
                       std::size_t height) const;

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
    /// Tries a straight Bresenham line first, so clear connections keep the
    /// original diagonal look. If that line would enter a room, the walkway
    /// falls back to a path that routes around the rooms instead.
    ///
    /// @throws std::out_of_range when either endpoint is outside the map.
    /// @throws std::runtime_error when no route between the endpoints exists.
    void walkway(std::size_t start_x, std::size_t start_y, std::size_t end_x, std::size_t end_y,
                 const std::vector<RoomSpec>& rooms);

    /// Fills one corner wherever the path touches another path tile diagonally.
    ///
    /// This keeps diagonal routes while preventing the two remaining corner
    /// cells from both staying floor. Only free floor cells outside @p rooms
    /// are filled.
    void fill_walkway_corners(const std::vector<RoomSpec>& rooms);
};

} // namespace dungeon::map
