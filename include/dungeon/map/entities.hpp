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

    Entity(char tile, Color color = Color::White, Style style = Style::None)
        : _tile(tile), _color(color), _style(style), _id(entity_id++) {}

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

class Alive {
public:
    struct Stats {
        std::size_t level = 1;
        std::size_t hp = 100;
        std::size_t attack = 10;
        std::size_t defence = 5;
        std::size_t stamina = 10;
        std::size_t gold = 0;
    };

    Alive(Stats stats) : _stats(stats) {}
    Alive(std::size_t hp, std::size_t attack, std::size_t defence, std::size_t stamina, std::size_t gold = 0)
        : _stats{1, hp, attack, defence, stamina, gold} {}

    [[nodiscard]] Stats stats() const { return _stats; }
    [[nodiscard]] bool alive() const { return _stats.hp > 0; }
    std::size_t level_up() { return ++_stats.level; }

private:
    Stats _stats;
};

class Player : public Entity, public Alive {
public:
    Player(std::size_t x, std::size_t y) : Entity('@', Color::White, Style::Bold), Alive(100, 15, 5, 10) {
        this->x = x;
        this->y = y;
    }
};

class Enemy : public Entity, public Alive {
public:
    Enemy(std::size_t x, std::size_t y) : Entity('E', Color::Red, Style::Bold), Alive(50, 8, 2, 5) {
        this->x = x;
        this->y = y;
    }
};

class Item : public Entity {
public:
    Item(char tile, Color color = Color::Yellow, Style style = Style::None)
        : Entity(tile, color, style) {}
    // TODO: action, pickable / interactable
};

class Coin : public Item {
public:
    Coin() : Item('$', Color::Yellow, Style::Bold) {}
};

class Key : public Item {
public:
    Key() : Item('k', Color::Cyan, Style::Bold) {}
};

class Chest : public Item {
public:
    Chest() : Item('?', Color::Purple, Style::Bold) {}
};

} // namespace dungeon::map::entities
