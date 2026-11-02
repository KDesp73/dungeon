#pragma once

#include "dungeon/display/style.hpp"

#include <cstddef>
#include <optional>
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

struct Stats {
    std::size_t level   = 1;
    int         hp      = 100;
    int         attack  = 10;
    int         defence = 5;
    int         stamina = 10;
    int         gold    = 0;
};

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

  private:
    Stats _stats;
};

class Enemy : public Entity, public Alive {
  public:
    Enemy(std::size_t x, std::size_t y)
        : Entity('&', Color::Red, Style::Bold, x, y), Alive(50, 8, 2, 5) {}
};

enum class ItemType : unsigned char {
    Equipable,
    Interactable
};

/// Item kinds the dungeon generator can place in a room.
enum class ItemKind : unsigned char {
    Coin,
    CoinDouble,
    Key,
    Chest,
    Diamond,
};

class Item : public Entity {
  public:
    Item(char tile, Color color, Style style, ItemType type, ItemKind kind, Stats stats,
         std::size_t x = 0, std::size_t y = 0)
        : Entity(tile, color, style, x, y), _stats_diff(stats), _type(type), _kind(kind) {}

    Item(char tile, Color color, Style style, ItemType type, ItemKind kind, int attack, int defence,
         int stamina, int gold, std::size_t x = 0, std::size_t y = 0)
        : Entity(tile, color, style, x, y),
          _stats_diff{0, 0, attack, defence, stamina, gold},
          _type(type),
          _kind(kind) {}

    [[nodiscard]] bool     equipable() const { return _type == ItemType::Equipable; }
    [[nodiscard]] bool     interactable() const { return _type == ItemType::Interactable; }
    [[nodiscard]] ItemKind kind() const { return _kind; }
    [[nodiscard]] int      gold() const { return _stats_diff.gold; }

    bool equip(Stats& target) {
        if (!equipable())
            return false;
        target.attack += _stats_diff.attack;
        target.defence += _stats_diff.defence;
        target.stamina += _stats_diff.stamina;
        target.gold += _stats_diff.gold;
        return true;
    }

    void drop(Stats& target) {
        target.attack -= _stats_diff.attack;
        target.defence -= _stats_diff.defence;
        target.stamina -= _stats_diff.stamina;
        target.gold -= _stats_diff.gold;
    }

  private:
    Stats    _stats_diff;
    ItemType _type;
    ItemKind _kind;
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

class Key : public Item {
  public:
    Key(std::size_t x, std::size_t y)
        : Item('k', Color::Cyan, Style::Bold, ItemType::Equipable, ItemKind::Key, stat_delta(), x,
               y) {}
};

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
        : Entity('@', Color::White, Style::Bold, x, y), Alive(100, 15, 5, 10) {}

    [[nodiscard]] const std::vector<Item>& inventory() const noexcept { return _inventory; }
    [[nodiscard]] bool                     has_item(ItemKind kind) const;
    [[nodiscard]] bool                     has_key() const { return has_item(ItemKind::Key); }

    /// Replaces the player's stats and inventory.
    ///
    /// Used when carrying the player into a freshly generated dungeon.
    void restore(Stats new_stats, std::vector<Item> new_inventory) {
        stats()    = new_stats;
        _inventory = std::move(new_inventory);
    }

    void equip(Item item);
    void drop(int id);
    void drop(ItemKind kind);

  private:
    std::vector<Item> _inventory;
};

} // namespace dungeon::map::entities
