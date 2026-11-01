#include "dungeon/map/entities.hpp"

#include <algorithm>
#include <utility>

namespace dungeon::map::entities {

void Entity::place(std::size_t xx, std::size_t yy) {
    this->x = xx;
    this->y = yy;
}

void Player::equip(Item item) {
    if (!item.equip(stats())) {
        return;
    }

    _inventory.push_back(std::move(item));
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
}

void Player::drop(int id) {
    const auto item = std::find_if(_inventory.begin(), _inventory.end(),
                                   [id](const Item& candidate) { return candidate.id() == id; });

    if (item == _inventory.end()) {
        return;
    }

    item->drop(stats());
    _inventory.erase(item);
}

bool Player::has_item(ItemKind kind) const {
    return std::any_of(_inventory.begin(), _inventory.end(),
                       [kind](const Item& item) { return item.kind() == kind; });
}

} // namespace dungeon::map::entities
