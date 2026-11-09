#include "dungeon/game.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace dungeon::game {

namespace {

/// Returns the Manhattan distance between @p a and @p b.
std::size_t grid_distance(const map::entities::Entity& a,
                          const map::entities::Entity& b) noexcept {
    const std::size_t dx = a.x > b.x ? a.x - b.x : b.x - a.x;
    const std::size_t dy = a.y > b.y ? a.y - b.y : b.y - a.y;
    return dx + dy;
}

/// Returns true when @p a and @p b are orthogonally adjacent.
bool one_square_apart(const map::entities::Entity& a, const map::entities::Entity& b) noexcept {
    return grid_distance(a, b) == 1;
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
    _chance.reseed(_map.seed());
    _chest_selection = 0;
    _game_over       = false;
    push_message("You enter the dungeon.");
}

void Game::generate(const map::MapSpec& spec) {
    _map.generate(spec);
    _chance.reseed(_map.seed());
    _chest_selection = 0;
    _game_over       = false;
    push_message("You enter the dungeon.");
}

void Game::restart() {
    const std::size_t width  = _map.width() > 0 ? _map.width() : 64;
    const std::size_t height = _map.height() > 0 ? _map.height() : 32;

    generate(width, height);
    push_message("You feel refreshed.");
}

bool Game::move(Direction direction) {
    auto* player = _map.player();
    if (player == nullptr || _game_over || !player->alive()) {
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

    if (auto* enemy = enemy_at(x, y); enemy != nullptr) {
        attack_enemy(*enemy);
        return true;
    }

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

void Game::update(double seconds) {
    if (seconds <= 0.0 || _game_over) {
        return;
    }

    auto* player = _map.player();
    if (player == nullptr) {
        return;
    }

    if (!player->alive()) {
        _game_over = true;
        return;
    }

    std::vector<map::entities::Enemy*> enemies;
    enemies.reserve(_map.entity_ptrs().size());

    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        auto* enemy = dynamic_cast<map::entities::Enemy*>(entity.get());
        if (enemy != nullptr && enemy->alive()) {
            enemies.push_back(enemy);
        }
    }

    for (map::entities::Enemy* enemy : enemies) {
        if (enemy == nullptr || !enemy->alive()) {
            continue;
        }

        enemy->advance_timers(seconds);

        if (grid_distance(*enemy, *player) <=
            static_cast<std::size_t>(enemy->profile().aggro_radius)) {
            enemy->alert();
        }

        if (one_square_apart(*enemy, *player)) {
            enemy->reset_move_timer();

            if (enemy->attack_ready()) {
                attack_player(*enemy);
                enemy->reset_attack_timer();
            }

            if (_game_over) {
                break;
            }

            continue;
        }

        if (!enemy->move_ready()) {
            continue;
        }

        if (enemy->alerted()) {
            chase_player(*enemy);
        } else {
            wander(*enemy);
        }

        enemy->reset_move_timer();
    }
}

map::entities::Enemy* Game::nearest_enemy() noexcept {
    return const_cast<map::entities::Enemy*>(std::as_const(*this).nearest_enemy());
}

const map::entities::Enemy* Game::nearest_enemy() const noexcept {
    const auto* player = _map.player();
    if (player == nullptr) {
        return nullptr;
    }

    const map::entities::Enemy* nearest = nullptr;
    std::size_t                 best    = std::numeric_limits<std::size_t>::max();

    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        const auto* enemy = dynamic_cast<const map::entities::Enemy*>(entity.get());
        if (enemy == nullptr || !enemy->alive()) {
            continue;
        }

        const std::size_t distance = grid_distance(*enemy, *player);
        if (distance < best) {
            best    = distance;
            nearest = enemy;
        }
    }

    return nearest;
}

map::entities::Enemy* Game::enemy_at(std::size_t x, std::size_t y) noexcept {
    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        auto* enemy = dynamic_cast<map::entities::Enemy*>(entity.get());
        if (enemy != nullptr && enemy->alive() && enemy->x == x && enemy->y == y) {
            return enemy;
        }
    }

    return nullptr;
}

bool Game::occupied(std::size_t x, std::size_t y,
                    const map::entities::Enemy* ignore) const noexcept {
    const auto* player = _map.player();
    if (player != nullptr && player->x == x && player->y == y) {
        return true;
    }

    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        const auto* enemy = dynamic_cast<const map::entities::Enemy*>(entity.get());
        if (enemy != nullptr && enemy != ignore && enemy->alive() && enemy->x == x &&
            enemy->y == y) {
            return true;
        }
    }

    return false;
}

void Game::attack_enemy(map::entities::Enemy& enemy) {
    auto* player = _map.player();
    if (player == nullptr || !player->alive() || !enemy.alive()) {
        return;
    }

    const int damage = std::max(1, player->stats().attack - enemy.stats().defence);
    enemy.take_damage(damage);

    if (enemy.alive()) {
        push_message("You hit the " + std::string(enemy.name()) + " for " +
                     std::to_string(damage) + ".");
        return;
    }

    const std::size_t drop_x     = enemy.x;
    const std::size_t drop_y     = enemy.y;
    const std::string enemy_name = std::string(enemy.name());
    const std::size_t coin_count = _chance.range(static_cast<std::size_t>(enemy.gold_min()),
                                                 static_cast<std::size_t>(enemy.gold_max()));

    static_cast<void>(_map.remove(enemy.id()));

    for (std::size_t i = 0; i < coin_count; ++i) {
        static_cast<void>(_map.add(std::make_shared<map::entities::Coin>(drop_x, drop_y)));
    }

    push_message("The " + enemy_name + " dies and drops " + std::to_string(coin_count) +
                 (coin_count == 1 ? " coin." : " coins."));
}

void Game::attack_player(map::entities::Enemy& enemy) {
    auto* player = _map.player();
    if (player == nullptr || !player->alive()) {
        return;
    }

    const int damage = std::max(1, enemy.stats().attack - player->stats().defence);
    player->stats().hp -= damage;
    if (player->stats().hp < 0) {
        player->stats().hp = 0;
    }

    push_message("The " + std::string(enemy.name()) + " hits you for " +
                 std::to_string(damage) + ".");

    if (!player->alive() && !_game_over) {
        _game_over = true;
        push_message("You have fallen. Press R to restart.");
    }
}

void Game::chase_player(map::entities::Enemy& enemy) {
    auto* player = _map.player();
    if (player == nullptr) {
        return;
    }

    const std::optional<Offset> step =
        first_step_toward(enemy.x, enemy.y, player->x, player->y, enemy);

    if (!step) {
        return;
    }

    const auto next_x = static_cast<std::ptrdiff_t>(enemy.x) + step->x;
    const auto next_y = static_cast<std::ptrdiff_t>(enemy.y) + step->y;

    if (next_x < 0 || next_y < 0) {
        return;
    }

    const auto x = static_cast<std::size_t>(next_x);
    const auto y = static_cast<std::size_t>(next_y);

    if (!_map.walkable(x, y) || occupied(x, y, &enemy)) {
        return;
    }

    enemy.place(x, y);
}

void Game::wander(map::entities::Enemy& enemy) {
    constexpr int dx[4] = {0, 1, 0, -1};
    constexpr int dy[4] = {-1, 0, 1, 0};

    const int start = enemy.wander_direction() % 4;

    for (int i = 0; i < 4; ++i) {
        const int direction = (start + i) % 4;
        const auto next_x   = static_cast<std::ptrdiff_t>(enemy.x) + dx[direction];
        const auto next_y   = static_cast<std::ptrdiff_t>(enemy.y) + dy[direction];

        if (next_x < 0 || next_y < 0) {
            continue;
        }

        const auto x = static_cast<std::size_t>(next_x);
        const auto y = static_cast<std::size_t>(next_y);

        if (!_map.walkable(x, y) || occupied(x, y, &enemy)) {
            continue;
        }

        enemy.place(x, y);
        enemy.set_wander_direction(direction);
        return;
    }

    enemy.set_wander_direction((start + 1) % 4);
}

std::optional<Offset> Game::first_step_toward(std::size_t start_x, std::size_t start_y,
                                              std::size_t goal_x, std::size_t goal_y,
                                              const map::entities::Enemy& mover) const {
    if (start_x >= _map.width() || start_y >= _map.height() || goal_x >= _map.width() ||
        goal_y >= _map.height()) {
        return std::nullopt;
    }

    const std::size_t width      = _map.width();
    const std::size_t cell_count = width * _map.height();
    const std::size_t start      = start_y * width + start_x;
    const std::size_t goal       = goal_y * width + goal_x;

    if (start == goal) {
        return std::nullopt;
    }

    constexpr int dx[4] = {0, 1, 0, -1};
    constexpr int dy[4] = {-1, 0, 1, 0};

    std::vector<std::ptrdiff_t> parent(cell_count, -1);
    std::queue<std::size_t>     pending;

    parent[start] = static_cast<std::ptrdiff_t>(start);
    pending.push(start);

    bool found = false;

    while (!pending.empty() && !found) {
        const std::size_t current = pending.front();
        pending.pop();

        const std::size_t x = current % width;
        const std::size_t y = current / width;

        for (int move = 0; move < 4; ++move) {
            const auto next_x = static_cast<std::ptrdiff_t>(x) + dx[move];
            const auto next_y = static_cast<std::ptrdiff_t>(y) + dy[move];

            if (next_x < 0 || next_y < 0 || next_x >= static_cast<std::ptrdiff_t>(_map.width()) ||
                next_y >= static_cast<std::ptrdiff_t>(_map.height())) {
                continue;
            }

            const auto nx    = static_cast<std::size_t>(next_x);
            const auto ny    = static_cast<std::size_t>(next_y);
            const auto index = ny * width + nx;

            if (parent[index] != -1 || !_map.walkable(nx, ny)) {
                continue;
            }

            if (index != goal && occupied(nx, ny, &mover)) {
                continue;
            }

            parent[index] = static_cast<std::ptrdiff_t>(current);
            pending.push(index);

            if (index == goal) {
                found = true;
                break;
            }
        }
    }

    if (!found) {
        return std::nullopt;
    }

    std::size_t step = goal;

    while (parent[step] != static_cast<std::ptrdiff_t>(start)) {
        if (parent[step] < 0) {
            return std::nullopt;
        }
        step = static_cast<std::size_t>(parent[step]);
    }

    return Offset{static_cast<int>(step % width) - static_cast<int>(start_x),
                  static_cast<int>(step / width) - static_cast<int>(start_y)};
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
