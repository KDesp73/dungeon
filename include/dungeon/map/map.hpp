#pragma once

#include <cstddef>
#include <cstdio>
#include <vector>

namespace dungeon::map {

/// Terrain occupying a single map cell.
enum class Tile : unsigned char {
    HWall,
    VWall,
    Walkway,
    Floor,
    Player,
    Key,
    Door,
};

/// ASCII glyph used to display a tile.
[[nodiscard]] char glyph(Tile tile) noexcept;

/// A rectangular tile map.
///
/// Tiles are stored in a single contiguous buffer in row-major order, so a map
/// of `width()` columns and `height()` rows contains `width() * height()` tiles.
class Map {
public:
    Map() = default;

    /// Builds a map with two rooms connected by a walkway.
    ///
    /// The rooms and walkway are omitted when the requested dimensions are too
    /// small to contain them, leaving an open floor map. The map never exceeds
    /// @p width by @p height.
    void generate(std::size_t width = 64, std::size_t height = 32);

    /// Removes every tile and resets the map to zero width and height.
    void clear();

    [[nodiscard]] bool is_wall(std::size_t index);
    [[nodiscard]] bool is_wall(std::size_t x, std::size_t y);

    [[nodiscard]] std::size_t width() const noexcept { return _width; }
    [[nodiscard]] std::size_t height() const noexcept { return _height; }
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

private:
    std::size_t _width    = 0;
    std::size_t _height   = 0;
    std::size_t _entrance = 0;
    std::size_t _exit     = 0;
    std::vector<Tile> _tiles;

    enum class DoorSide {
        Top,
        Right,
        Bottom,
        Left,
    };

    struct Door {
        std::size_t x;
        std::size_t y;
    };

    /// Draws a rectangular room whose top-left wall is at (@p x, @p y).
    ///
    /// @p width and @p height include the walls, so the interior is
    /// `(width - 2) x (height - 2)` tiles and is filled with `Tile::Floor`.
    /// A single `Tile::Door` is placed in the middle of @p door_side and its
    /// position is returned.
    ///
    /// @throws std::invalid_argument when the room is smaller than 3x3.
    /// @throws std::out_of_range when the room does not fit inside the map.
    [[nodiscard]] Door room(std::size_t x, std::size_t y, std::size_t width, std::size_t height,
                            DoorSide door_side = DoorSide::Right);

    /// Carves a straight line of `Tile::Walkway` from one cell to another.
    ///
    /// Uses Bresenham's algorithm, so the line is connected horizontally,
    /// vertically, or diagonally. Any tile already on the line is replaced
    /// except for doors, which are preserved as connection points.
    ///
    /// @throws std::out_of_range when either endpoint is outside the map.
    void walkway(std::size_t start_x, std::size_t start_y, std::size_t end_x, std::size_t end_y);
};

} // namespace dungeon::map
