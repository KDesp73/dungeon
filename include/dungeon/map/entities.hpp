#pragma once

#include <cstddef>

namespace dungeon::map::entities {

enum class Color : unsigned char {
    Red, Blue, Green, Yellow, Grey, Black, White, Cyan, Purple
};

enum class Style : unsigned char {
    None, Underline, Bold, Italic
};

class Entity {
    inline static int entity_id = 1;
public:
    std::size_t x = 0;
    std::size_t y = 0;

    Entity(
        char tile,
        Color color = Color::White,
        Style style = Style::None,
        std::size_t x = 0, std::size_t y = 0)
        : x(x), y(y),
          _tile(tile), _color(color), _style(style), _id(entity_id++) {}

    virtual ~Entity() = default; // Good practice for polymorphic base classes

    [[nodiscard]] int id() const { return _id; }
    [[nodiscard]] char tile() const { return _tile; }
    [[nodiscard]] Color color() const { return _color; }
    void hide() { _visible = false; }
    void show() { _visible = true; }
    void place(std::size_t xx, std::size_t yy);

private:
    char _tile;
    Color _color;
    Style _style;
    bool _visible = true;
    int _id;
};

struct Stats {
    std::size_t level = 1;
    int hp = 100;
    int attack = 10;
    int defence = 5;
    int stamina = 10;
    int gold = 0;
};

class Alive {
public:
    Alive(Stats stats) : _stats(stats) {}
    Alive(int hp, int attack, int defence, int stamina, int gold = 0)
        : _stats{1, hp, attack, defence, stamina, gold} {}

    [[nodiscard]] Stats stats() const { return _stats; }
    [[nodiscard]] bool alive() const { return _stats.hp > 0; }
    std::size_t level_up() { return ++_stats.level; }

private:
    Stats _stats;
};

class Player : public Entity, public Alive {
public:
    Player(std::size_t x, std::size_t y)
        : Entity('@', Color::White, Style::Bold, x, y),
          Alive(100, 15, 5, 10) {}
};

class Enemy : public Entity, public Alive {
public:
    Enemy(std::size_t x, std::size_t y)
        : Entity('E', Color::Red, Style::Bold, x, y),
          Alive(50, 8, 2, 5) {}
};

enum class ItemType : unsigned char {
    Equipable, Interactable
};

class Item : public Entity {
public:
    Item(char tile, Color color, Style style, ItemType type, Stats stats, std::size_t x = 0, std::size_t y = 0)
        : Entity(tile, color, style, x, y), _stats_diff(stats), _type(type) {}

    Item(char tile, Color color, Style style, ItemType type, int attack, int defence, int stamina, int gold, std::size_t x = 0, std::size_t y = 0)
        : Entity(tile, color, style, x, y), _stats_diff{0, 0, attack, defence, stamina, gold}, _type(type) {}

    [[nodiscard]] bool equipable() const { return _type == ItemType::Equipable; }
    [[nodiscard]] bool interactable() const { return _type == ItemType::Interactable; }

    bool equip(Stats& target) {
        if (!equipable()) return false;
        target.attack  += _stats_diff.attack;
        target.defence += _stats_diff.defence;
        target.stamina += _stats_diff.stamina;
        target.gold    += _stats_diff.gold;
        return true;
    }

    void leave(Stats& target) {
        target.attack  -= _stats_diff.attack;
        target.defence -= _stats_diff.defence;
        target.stamina -= _stats_diff.stamina;
        target.gold    -= _stats_diff.gold;
    }

private:
    Stats _stats_diff;
    ItemType _type;
};

class Coin : public Item {
public:
    Coin(std::size_t x, std::size_t y)
        : Item('$', Color::Yellow, Style::Bold, ItemType::Equipable, Stats{.gold = 1}, x, y) {}
};

class Key : public Item {
public:
    Key(std::size_t x, std::size_t y)
        : Item('k', Color::Cyan, Style::Bold, ItemType::Equipable, Stats{}, x, y) {}
};

class Chest : public Item {
public:
    Chest(std::size_t x, std::size_t y)
        : Item('?', Color::Purple, Style::Bold, ItemType::Interactable, Stats{}, x, y) {}
};

} // namespace dungeon::map::entities
