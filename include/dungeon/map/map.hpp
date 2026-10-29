#pragma once

#include <cstddef>
#include <cstdio>
#include <vector>

namespace dungeon::map {

/// Terrain occupying a single map cell.
enum class Tile : unsigned char {
    Wall,
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

    /// Replaces the map with an empty room of the requested size.
    ///
    /// The outer ring of tiles is always wall; the interior is floor. Passing a
    /// width or height of zero clears the map. The default size is 32x32.
    void generate(std::size_t width = 32, std::size_t height = 32);

    /// Removes every tile and resets the map to zero width and height.
    void clear();

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
    std::size_t _width  = 0;
    std::size_t _height = 0;
    std::vector<Tile> _tiles;
};

} // namespace dungeon::map
