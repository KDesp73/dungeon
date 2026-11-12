#include "dungeon/map/entities.hpp"

#include <algorithm>
#include <utility>

namespace dungeon::map::entities {

Item make_item(ItemKind kind, std::size_t x, std::size_t y) {
    switch (kind) {
    case ItemKind::Coin:
        return Coin(x, y);
    case ItemKind::CoinDouble:
        return CoinDouble(x, y);
    case ItemKind::Diamond:
        return Diamond(x, y);
    case ItemKind::HealthPotion:
        return HealthPotion(x, y);
    case ItemKind::Key:
        return Key(x, y);
    case ItemKind::Sword:
        return Sword(x, y);
    case ItemKind::Greatsword:
        return Greatsword(x, y);
    case ItemKind::Shield:
        return Shield(x, y);
    case ItemKind::Chainmail:
        return Chainmail(x, y);
    case ItemKind::Boots:
        return Boots(x, y);
    case ItemKind::Chest:
        break;
    }

    return Coin(x, y);
}

void Entity::place(std::size_t xx, std::size_t yy) {
    this->x = xx;
    this->y = yy;
}

bool Player::equip(Item item) {
    if (item.upgradable()) {
        const auto existing =
            std::find_if(_inventory.begin(), _inventory.end(), [&item](const Item& candidate) {
                return candidate.kind() == item.kind();
            });

        if (existing != _inventory.end()) {
            return existing->upgrade(stats());
        }
    }

    if (inventory_full()) {
        return false;
    }

    if (!item.equip(stats())) {
        return false;
    }

    _inventory.push_back(std::move(item));
    return true;
}

void Player::drop(ItemKind kind) {
    const auto item =
        std::find_if(_inventory.begin(), _inventory.end(),
                     [kind](const Item& candidate) { return candidate.kind() == kind; });

    if (item == _inventory.end()) {
        return;
    }

    item->drop(stats());
    _inventory.erase(item);

    const int cap = max_hp(stats());
    if (stats().hp > cap) {
        stats().hp = cap;
    }
}

void Player::drop(int id) {
    const auto item = std::find_if(_inventory.begin(), _inventory.end(),
                                   [id](const Item& candidate) { return candidate.id() == id; });

    if (item == _inventory.end()) {
        return;
    }

    item->drop(stats());
    _inventory.erase(item);

    const int cap = max_hp(stats());
    if (stats().hp > cap) {
        stats().hp = cap;
    }
}

bool Player::has_item(ItemKind kind) const {
    return std::any_of(_inventory.begin(), _inventory.end(),
                       [kind](const Item& item) { return item.kind() == kind; });
}

} // namespace dungeon::map::entities
