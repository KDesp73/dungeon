#include "dungeon/game.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace dungeon::game {

namespace {

/// Returns true when @p a and @p b are orthogonally adjacent.
bool one_square_apart(const map::entities::Entity& a, const map::entities::Entity& b) noexcept {
    const std::size_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const std::size_t dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx + dy == 1;
}

/// Returns a short human readable name for @p kind.
std::string item_name(map::entities::ItemKind kind) {
    switch (kind) {
    case map::entities::ItemKind::Coin:
        return "coin";
    case map::entities::ItemKind::CoinDouble:
        return "double coin";
    case map::entities::ItemKind::Diamond:
        return "diamond";
    case map::entities::ItemKind::Key:
        return "key";
    case map::entities::ItemKind::Chest:
        return "chest";
    }
    return "item";
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
    push_message("You enter the dungeon.");
}

void Game::generate(const map::MapSpec& spec) {
    _map.generate(spec);
    push_message("You enter the dungeon.");
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

    push_message("You descend deeper.");
}

const map::entities::Chest* Game::nearby_chest() const noexcept {
    const auto* player = _map.player();
    if (player == nullptr) {
        return nullptr;
    }

    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        const auto* chest = dynamic_cast<const map::entities::Chest*>(entity.get());
        if (chest == nullptr) {
            continue;
        }

        const std::size_t dx = chest->x > player->x ? chest->x - player->x : player->x - chest->x;
        const std::size_t dy = chest->y > player->y ? chest->y - player->y : player->y - chest->y;

        if (dx + dy <= 1) {
            return chest;
        }
    }

    return nullptr;
}

map::entities::Chest* Game::nearby_chest() noexcept {
    return const_cast<map::entities::Chest*>(std::as_const(*this).nearby_chest());
}

void Game::cycle_chest_selection(int delta) noexcept {
    const auto* chest = nearby_chest();
    const std::size_t count = chest != nullptr ? chest->contents().size() : 0;

    if (count == 0) {
        _chest_selection = 0;
        return;
    }

    if (_chest_selection >= count) {
        _chest_selection = 0;
    }

    if (delta > 0) {
        _chest_selection = (_chest_selection + 1) % count;
    } else if (delta < 0) {
        _chest_selection = (_chest_selection + count - 1) % count;
    }
}

void Game::select_chest_item(std::size_t index) noexcept {
    const auto* chest = nearby_chest();

    if (chest == nullptr || chest->contents().empty()) {
        _chest_selection = 0;
        return;
    }

    _chest_selection = std::min(index, chest->contents().size() - 1);
}

bool Game::take_selected_chest_item() {
    auto* player = _map.player();
    auto* chest  = nearby_chest();

    if (player == nullptr || chest == nullptr) {
        return false;
    }

    if (chest->empty()) {
        push_message("The chest is empty.");
        return false;
    }

    if (_chest_selection >= chest->contents().size()) {
        _chest_selection = chest->contents().size() - 1;
    }

    std::optional<map::entities::Item> item = chest->take(_chest_selection);
    if (!item) {
        return false;
    }

    if (item->kind() == map::entities::ItemKind::Coin ||
        item->kind() == map::entities::ItemKind::CoinDouble ||
        item->kind() == map::entities::ItemKind::Diamond) {
        const int gold = item->gold();
        static_cast<void>(item->equip(player->stats()));
        push_message("+" + std::to_string(gold) + " gold");
    } else {
        const map::entities::ItemKind kind = item->kind();
        player->equip(*item);
        push_message("Took the " + item_name(kind) + ".");
    }

    if (chest->contents().empty()) {
        _chest_selection = 0;
    } else if (_chest_selection >= chest->contents().size()) {
        _chest_selection = chest->contents().size() - 1;
    }

    return true;
}

void Game::push_message(std::string message) {
    constexpr std::size_t kMaxMessages = 50;

    if (_messages.size() >= kMaxMessages) {
        _messages.erase(_messages.begin());
    }

    _messages.push_back(std::move(message));
}

void Game::apply_step_action(const ActionEvent& event) {
    if (event.trigger != ActionTrigger::Step) {
        return;
    }

    switch (event.item.kind()) {
    case map::entities::ItemKind::Coin:
    case map::entities::ItemKind::CoinDouble:
    case map::entities::ItemKind::Diamond: {
        const int gold = event.item.gold();
        static_cast<void>(event.item.equip(event.player.stats()));
        static_cast<void>(_map.remove(event.item.id()));
        push_message("+" + std::to_string(gold) + " gold");
        break;
    }
    case map::entities::ItemKind::Key:
        event.player.equip(event.item);
        static_cast<void>(_map.remove(event.item.id()));
        push_message("Key acquired.");
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
