#include "dungeon/game.hpp"

#include <cstddef>
#include <vector>

namespace dungeon::game {

namespace {

/// Returns true when @p a and @p b are orthogonally adjacent.
bool one_square_apart(const map::entities::Entity& a, const map::entities::Entity& b) noexcept {
    const std::size_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const std::size_t dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx + dy == 1;
}

} // namespace

Offset offset(Direction direction) noexcept {
    switch (direction) {
    case Direction::Up:
        return {0, -1};
    case Direction::Down:
        return {0, 1};
    case Direction::Left:
        return {-1, 0};
    case Direction::Right:
        return {1, 0};
    }

    return {};
}

std::optional<Direction> direction_from_key(char key) noexcept {
    switch (key) {
    case 'w':
    case 'W':
        return Direction::Up;
    case 's':
    case 'S':
        return Direction::Down;
    case 'a':
    case 'A':
        return Direction::Left;
    case 'd':
    case 'D':
        return Direction::Right;
    default:
        return std::nullopt;
    }
}

std::optional<Direction> direction_from_key(const controls::KeyEvent& key) noexcept {
    switch (key.kind) {
    case controls::KeyKind::Up:
        return Direction::Up;
    case controls::KeyKind::Down:
        return Direction::Down;
    case controls::KeyKind::Left:
        return Direction::Left;
    case controls::KeyKind::Right:
        return Direction::Right;
    case controls::KeyKind::Character:
        return direction_from_key(key.value);
    case controls::KeyKind::None:
    case controls::KeyKind::Escape:
        return std::nullopt;
    }

    return std::nullopt;
}

Game::Game(std::size_t width, std::size_t height) {
    generate(width, height);
}

Game::Game(const map::MapSpec& spec) {
    generate(spec);
}

void Game::generate(std::size_t width, std::size_t height) {
    _map.generate(width, height);
}

void Game::generate(const map::MapSpec& spec) {
    _map.generate(spec);
}

bool Game::move(Direction direction) {
    auto* player = _map.player();
    if (player == nullptr) {
        return false;
    }

    const Offset delta  = offset(direction);
    const auto   next_x = static_cast<std::ptrdiff_t>(player->x) + delta.x;
    const auto   next_y = static_cast<std::ptrdiff_t>(player->y) + delta.y;

    if (next_x < 0 || next_y < 0) {
        return false;
    }

    const auto x = static_cast<std::size_t>(next_x);
    const auto y = static_cast<std::size_t>(next_y);

    if (!_map.walkable(x, y)) {
        return false;
    }

    player->place(x, y);
    dispatch_actions();

    auto* current = _map.player();
    if (current != nullptr && _map.exit_unlocked() && current->x == _map.exit().x &&
        current->y == _map.exit().y) {
        advance_level();
    }

    return true;
}

void Game::advance_level() {
    const auto* player = _map.player();
    if (player == nullptr) {
        return;
    }

    const std::size_t width     = _map.width();
    const std::size_t height    = _map.height();
    const auto        stats     = player->stats();
    const auto        inventory = player->inventory();

    generate(width, height);

    if (auto* next = _map.player(); next != nullptr) {
        next->restore(stats, inventory);
        next->drop(map::entities::ItemKind::Key);
    }
}

void Game::apply_step_action(const ActionEvent& event) {
    if (event.trigger != ActionTrigger::Step) {
        return;
    }

    switch (event.item.kind()) {
    case map::entities::ItemKind::Coin:
    case map::entities::ItemKind::CoinDouble:
        static_cast<void>(event.item.equip(event.player.stats()));
        static_cast<void>(_map.remove(event.item.id()));
        break;
    case map::entities::ItemKind::Key:
        event.player.equip(event.item);
        static_cast<void>(_map.remove(event.item.id()));
        break;
    case map::entities::ItemKind::Chest:
        break;
    }
}

void Game::dispatch_actions() {
    auto* player = _map.player();
    if (player == nullptr) {
        return;
    }

    struct PendingAction {
        ActionTrigger        trigger;
        map::EntityPtr       owner;
        map::entities::Item* item;
    };

    std::vector<PendingAction> pending;
    pending.reserve(_map.entity_ptrs().size());

    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        if (!entity || entity.get() == player) {
            continue;
        }

        auto* item = dynamic_cast<map::entities::Item*>(entity.get());
        if (item == nullptr) {
            continue;
        }

        if (item->x == player->x && item->y == player->y) {
            pending.push_back(PendingAction{ActionTrigger::Step, entity, item});
        } else if (item->interactable() && one_square_apart(*item, *player)) {
            pending.push_back(PendingAction{ActionTrigger::Nearby, entity, item});
        }
    }

    for (const PendingAction& action : pending) {
        auto* current_player = _map.player();
        if (current_player == nullptr) {
            break;
        }

        const ActionEvent event{action.trigger, *action.item, *current_player};
        apply_step_action(event);

        if (_action_hook) {
            _action_hook(event);
        }
    }
}

} // namespace dungeon::game
