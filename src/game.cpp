#include "dungeon/game.hpp"

#include <algorithm>
#include <array>
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
    case map::entities::ItemKind::HealthPotion:
        return "health potion";
    case map::entities::ItemKind::Sword:
        return "sword";
    case map::entities::ItemKind::Greatsword:
        return "greatsword";
    case map::entities::ItemKind::Shield:
        return "shield";
    case map::entities::ItemKind::Chainmail:
        return "chainmail";
    case map::entities::ItemKind::Boots:
        return "boots";
    case map::entities::ItemKind::Chest:
        return "chest";
    }
    return "item";
}

/// The upgrades sold in every store level.
std::span<const StoreItem> store_catalog() noexcept {
    static const StoreItem items[] = {
        {map::entities::ItemKind::Sword, "Sword", "+2 attack", 10},
        {map::entities::ItemKind::Shield, "Shield", "+2 defence", 12},
        {map::entities::ItemKind::Chainmail, "Chainmail", "+3 defence, +1 stamina (+2 HP)", 22},
        {map::entities::ItemKind::Greatsword, "Greatsword", "+5 attack", 35},
        {map::entities::ItemKind::Boots, "Boots", "+2 stamina (+4 HP)", 8},
    };

    return items;
}

/// Returns the store catalog with prices raised for the given store visit.
///
/// The first visit uses the base prices. Every later visit adds
/// `kStorePriceGrowthPercent` of each base price, rounded to the nearest gold.
std::vector<StoreItem> store_catalog_for_visit(std::size_t visit) {
    std::vector<StoreItem> items(store_catalog().begin(), store_catalog().end());
    const long long        steps = visit > 1 ? static_cast<long long>(visit - 1) : 0;

    for (StoreItem& item : items) {
        const long long increase =
            (static_cast<long long>(item.price) * kStorePriceGrowthPercent * steps + 50) / 100;
        item.price += static_cast<int>(increase);
    }

    return items;
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
    _depth           = 1;
    _store_level     = false;
    _store_open      = false;
    _boss_level      = false;
    _victory         = false;
    _giggle_elapsed  = 0.0;
    _giggle_hold     = 0.0;
    _store_items.clear();
    _store_sold.clear();
    _store_visits = 0;
    _run_stats    = {};
    _game_over    = false;
    push_message("You enter the dungeon.");
}

void Game::generate(const map::MapSpec& spec) {
    _map.generate(spec);
    _chance.reseed(_map.seed());
    _chest_selection = 0;
    _depth           = 1;
    _store_level     = false;
    _store_open      = false;
    _boss_level      = spec.has_boss;
    _victory         = false;
    _giggle_elapsed  = 0.0;
    _giggle_hold     = 0.0;
    _store_items.clear();
    _store_sold.clear();
    _store_visits = 0;
    _run_stats    = {};
    _game_over    = false;
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
    if (player == nullptr || _game_over || _victory || !player->alive()) {
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

    if (auto* anchor = anchor_at(x, y); anchor != nullptr) {
        attack_anchor(*anchor);
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
        descend();
    }

    return true;
}

void Game::update(double seconds) {
    if (seconds <= 0.0 || _game_over || _victory) {
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

        if (auto* boss = dynamic_cast<map::entities::Boss*>(enemy); boss != nullptr) {
            update_boss(*boss, seconds);
            if (_game_over) {
                break;
            }
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

    if (!_game_over) {
        update_fireballs(seconds);
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

map::entities::Boss* Game::boss() noexcept {
    return const_cast<map::entities::Boss*>(std::as_const(*this).boss());
}

const map::entities::Boss* Game::boss() const noexcept {
    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        const auto* candidate = dynamic_cast<const map::entities::Boss*>(entity.get());
        if (candidate != nullptr && candidate->alive()) {
            return candidate;
        }
    }
    return nullptr;
}

std::size_t Game::anchors_remaining() const noexcept {
    std::size_t count = 0;
    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        const auto* anchor = dynamic_cast<const map::entities::Anchor*>(entity.get());
        if (anchor != nullptr && anchor->alive()) {
            ++count;
        }
    }
    return count;
}

bool Game::use_health_potion() {
    auto* player = _map.player();
    if (player == nullptr || _game_over || !player->alive()) {
        return false;
    }

    if (!player->has_item(map::entities::ItemKind::HealthPotion)) {
        push_message("You have no health potions.");
        return false;
    }

    const int hp = player->stats().hp;
    const int cap = map::entities::max_hp(player->stats());
    if (hp >= cap) {
        push_message("You are already at full health.");
        return false;
    }

    const auto rolled = _chance.range(
        static_cast<std::size_t>(map::entities::HEALTH_POTION_HEAL_MIN),
        static_cast<std::size_t>(map::entities::HEALTH_POTION_HEAL_MAX));
    const int healed = std::min(static_cast<int>(rolled), cap - hp);

    player->stats().hp += healed;
    player->drop(map::entities::ItemKind::HealthPotion);
    ++_run_stats.potions_used;
    push_message("You drink a health potion and recover " + std::to_string(healed) +
                 " health.");
    return true;
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

map::entities::Anchor* Game::anchor_at(std::size_t x, std::size_t y) noexcept {
    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        auto* anchor = dynamic_cast<map::entities::Anchor*>(entity.get());
        if (anchor != nullptr && anchor->alive() && anchor->x == x && anchor->y == y) {
            return anchor;
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

    auto* boss = dynamic_cast<map::entities::Boss*>(&enemy);
    if (boss != nullptr && !boss->vulnerable()) {
        push_message("The boss is shielded. Destroy the anchors first.");
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
    // Removing the enemy destroys it, so read boss state before that happens.
    const bool final_boss = boss != nullptr && boss->is_final();

    static_cast<void>(_map.remove(enemy.id()));
    ++_run_stats.enemies_killed;

    for (std::size_t i = 0; i < coin_count; ++i) {
        static_cast<void>(_map.add(std::make_shared<map::entities::Coin>(drop_x, drop_y)));
    }

    push_message("The " + enemy_name + " dies and drops " + std::to_string(coin_count) +
                 (coin_count == 1 ? " coin." : " coins."));

    if (boss != nullptr) {
        on_boss_defeated(final_boss, drop_x, drop_y);
    }
}

void Game::attack_anchor(map::entities::Anchor& anchor) {
    auto* player = _map.player();
    if (player == nullptr || !player->alive()) {
        return;
    }

    anchor.take_damage();
    if (anchor.alive()) {
        push_message("You strike an anchor (" + std::to_string(anchor.hits()) +
                     (anchor.hits() == 1 ? " hit left)." : " hits left)."));
        return;
    }

    static_cast<void>(_map.remove(anchor.id()));

    auto* boss = this->boss();
    if (boss == nullptr) {
        push_message("The anchor crumbles.");
        return;
    }

    const std::size_t remaining = boss->anchors_remaining() > 0 ? boss->anchors_remaining() - 1 : 0;
    boss->set_anchors_remaining(remaining);

    if (remaining == 0) {
        push_message("Every anchor is destroyed. The boss is vulnerable!");
    } else {
        push_message("An anchor crumbles. " + std::to_string(remaining) +
                     (remaining == 1 ? " remains." : " remain."));
    }
}

void Game::attack_player(map::entities::Enemy& enemy) {
    damage_player(enemy.stats().attack, "The " + std::string(enemy.name()));
}

void Game::damage_player(int damage, const std::string& source) {
    auto* player = _map.player();
    if (player == nullptr || !player->alive()) {
        return;
    }

    const int reduced = std::max(1, damage - player->stats().defence);
    player->stats().hp -= reduced;
    if (player->stats().hp < 0) {
        player->stats().hp = 0;
    }

    push_message(source + " hits you for " + std::to_string(reduced) + ".");

    if (!player->alive() && !_game_over) {
        _game_over = true;
        push_message("You have fallen. Press R to restart.");
    }
}

void Game::on_boss_defeated(bool final_boss, std::size_t x, std::size_t y) {
    ++_run_stats.bosses_defeated;
    push_message("The boss is defeated!");

    if (final_boss) {
        // The final key is handed over with the victory rather than left on the
        // floor: the arena has no exit beyond the ending.
        ++_run_stats.keys_collected;
        _victory = true;
        push_message("You claim the fiftieth key. Victory!");
        return;
    }

    static_cast<void>(_map.add(std::make_shared<map::entities::Key>(x, y)));
    push_message("A key lies where the boss fell.");
}

void Game::update_boss(map::entities::Boss& boss, double seconds) {
    boss.advance_fireball_timer(seconds);

    _giggle_elapsed += seconds;
    if (_giggle_elapsed >= dungeon::map::entities::BOSS_GIGGLE_INTERVAL) {
        _giggle_elapsed = 0.0;
        _giggle_hold    = dungeon::map::entities::BOSS_GIGGLE_DURATION;
        boss.set_giggling(true);
        push_message("The " + std::string(boss.name()) + " giggles.");
    } else if (_giggle_hold > 0.0) {
        _giggle_hold -= seconds;
        if (_giggle_hold <= 0.0) {
            _giggle_hold = 0.0;
            boss.set_giggling(false);
        }
    }

    if (!boss.fireball_ready()) {
        return;
    }

    boss.reset_fireball_timer();

    // The opening volley is deterministic so the boss introduces its signature
    // pattern; every later volley is rolled from the tier's weights. The final
    // boss has the fullest mix, including the aimed burst.
    const auto weights = map::entities::boss_attack_weights(boss.tier(), boss.is_final());
    const auto pattern = boss.volleys_fired() == 0
                             ? boss.opening_attack()
                             : static_cast<map::entities::BossAttack>(_chance.weighted(weights));
    boss.set_attack_pattern(pattern);
    boss.record_volley();
    spawn_fireballs(boss);
}

void Game::spawn_fireballs(map::entities::Boss& boss) {
    // Clockwise from up, so a rotating fan sweeps smoothly around the arena.
    constexpr int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    constexpr int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    // Positions of the four cardinal directions in the clockwise array.
    constexpr std::size_t cardinal[4] = {0, 2, 4, 6};

    using map::entities::BossAttack;

    const auto* player  = _map.player();
    const auto  count   = boss.fireball_directions();
    const auto  pattern = boss.attack_pattern();

    std::array<std::size_t, 8> directions{};
    std::size_t                arms = 0;

    if (pattern == BossAttack::Spiral) {
        arms = boss.volley_size();
        const auto rotation = boss.volley_rotation();
        boss.advance_volley_rotation();
        for (std::size_t i = 0; i < arms; ++i) {
            const auto slot = (rotation + i) % count;
            directions[i]   = count == 4 ? cardinal[slot] : slot;
        }
    } else if (pattern == BossAttack::Aimed && player != nullptr) {
        const int sign_x = player->x > boss.x ? 1 : (player->x < boss.x ? -1 : 0);
        const int sign_y = player->y > boss.y ? 1 : (player->y < boss.y ? -1 : 0);

        std::size_t centre = 0;
        for (std::size_t dir = 0; dir < 8; ++dir) {
            if (dx[dir] == sign_x && dy[dir] == sign_y) {
                centre = dir;
                break;
            }
        }

        arms = map::entities::BOSS_AIMED_ARMS;
        for (std::size_t i = 0; i < arms; ++i) {
            directions[i] = (centre + 7 + i) % 8; // one left, centre, one right
        }
    } else {
        // Ring, or an aimed pattern before the player exists.
        arms = count;
        for (std::size_t slot = 0; slot < count; ++slot) {
            directions[slot] = count == 4 ? cardinal[slot] : slot;
        }
    }

    for (std::size_t i = 0; i < arms; ++i) {
        const auto step_x = dx[directions[i]];
        const auto step_y = dy[directions[i]];

        const auto next_x = static_cast<std::ptrdiff_t>(boss.x) + step_x;
        const auto next_y = static_cast<std::ptrdiff_t>(boss.y) + step_y;

        if (next_x < 0 || next_y < 0 || next_x >= static_cast<std::ptrdiff_t>(_map.width()) ||
            next_y >= static_cast<std::ptrdiff_t>(_map.height())) {
            continue;
        }

        const auto x = static_cast<std::size_t>(next_x);
        const auto y = static_cast<std::size_t>(next_y);

        if (!_map.walkable(x, y)) {
            continue;
        }

        // A fireball materialising on the player hits immediately.
        if (player != nullptr && player->x == x && player->y == y) {
            damage_player(boss.stats().attack, "A fireball");
            if (_game_over) {
                return;
            }
            continue;
        }

        static_cast<void>(_map.add(std::make_shared<map::entities::Fireball>(
            x, y, step_x, step_y, boss.stats().attack, boss.fireball_range(),
            boss.fireball_step_interval())));
    }
}

void Game::update_fireballs(double seconds) {
    std::vector<int> spent;

    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        auto* fireball = dynamic_cast<map::entities::Fireball*>(entity.get());
        if (fireball == nullptr) {
            continue;
        }

        fireball->advance(seconds);
        if (!fireball->step_ready()) {
            continue;
        }

        fireball->reset_step_timer();
        fireball->consume_step();

        const auto next_x = static_cast<std::ptrdiff_t>(fireball->x) + fireball->dx();
        const auto next_y = static_cast<std::ptrdiff_t>(fireball->y) + fireball->dy();

        const bool out_of_bounds =
            next_x < 0 || next_y < 0 || next_x >= static_cast<std::ptrdiff_t>(_map.width()) ||
            next_y >= static_cast<std::ptrdiff_t>(_map.height());

        const auto x = out_of_bounds ? std::size_t{0} : static_cast<std::size_t>(next_x);
        const auto y = out_of_bounds ? std::size_t{0} : static_cast<std::size_t>(next_y);

        const auto* player = _map.player();
        const bool hits_player =
            !out_of_bounds && player != nullptr && player->x == x && player->y == y;
        const bool hits_anchor = !out_of_bounds && anchor_at(x, y) != nullptr;
        const bool hits_boss   = !out_of_bounds && enemy_at(x, y) != nullptr;

        if (fireball->expired() || out_of_bounds || !_map.walkable(x, y) || hits_anchor ||
            hits_boss) {
            spent.push_back(fireball->id());
            continue;
        }

        if (hits_player) {
            damage_player(fireball->damage(), "A fireball");
            spent.push_back(fireball->id());
            if (_game_over) {
                break;
            }
            continue;
        }

        fireball->place(x, y);
    }

    for (const int id : spent) {
        static_cast<void>(_map.remove(id));
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

void Game::descend() {
    const auto* player = _map.player();
    if (player == nullptr) {
        return;
    }

    const std::size_t width      = _map.width() > 0 ? _map.width() : 64;
    const std::size_t height     = _map.height() > 0 ? _map.height() : 32;
    const auto        stats      = player->stats();
    const auto        inventory  = player->inventory();
    const std::size_t next_depth = _depth + 1;

    bool boss_level  = next_depth % kBossInterval == 0;
    bool final_boss  = boss_level && next_depth == kFinalBossDepth;
    bool store_level = !boss_level && next_depth % kStoreInterval == 0;

    if (boss_level) {
        const std::size_t tier = next_depth / kBossInterval;
        map::MapSpec      spec = _map.make_boss_spec(width, height, tier, final_boss);

        if (spec.rooms.empty()) {
            // Tiny explicit layouts cannot hold an arena; fall back to a normal
            // dungeon rather than stranding the player in an empty map.
            spec        = _map.make_spec(width, height, next_depth);
            boss_level  = false;
            final_boss  = false;
            store_level = next_depth % kStoreInterval == 0;
        }

        _map.generate(spec);
        _store_items.clear();
        _store_sold.clear();
    } else if (store_level) {
        map::MapSpec spec = _map.make_store_spec(width, height);

        if (spec.rooms.empty()) {
            spec        = _map.make_spec(width, height, next_depth);
            store_level = false;
        } else {
            ++_store_visits;
            _store_items = store_catalog_for_visit(_store_visits);
        }

        _map.generate(spec);
        _store_sold.assign(_store_items.size(), false);
    } else {
        _map.generate(width, height, next_depth);
        _store_items.clear();
        _store_sold.clear();
    }

    _depth           = next_depth;
    _store_level     = store_level;
    _store_open      = false;
    _boss_level      = boss_level;
    _victory         = false;
    _giggle_elapsed  = 0.0;
    _giggle_hold     = 0.0;
    _chest_selection = 0;
    ++_run_stats.dungeons_completed;

    scale_enemies_for_depth();

    const bool level_up = _depth % kLevelUpInterval == 0;

    if (auto* next = _map.player(); next != nullptr) {
        next->restore(stats, inventory);
        next->drop(map::entities::ItemKind::Key);

        if (level_up) {
            const std::size_t level = next->level_up();
            next->stats().attack += map::entities::LEVEL_UP_ATTACK_BONUS;
            next->stats().defence += map::entities::LEVEL_UP_DEFENCE_BONUS;
            next->stats().stamina += map::entities::LEVEL_UP_STAMINA_BONUS;
            next->stats().hp = map::entities::max_hp(next->stats());

            push_message("You reached level " + std::to_string(level) + ".");
        }
    }

    push_message("You descend deeper.");

    if (boss_level) {
        push_message(final_boss ? "The final boss blocks your way!"
                                : "A boss blocks your way!");
        if (anchors_remaining() > 0) {
            push_message("Destroy the anchors to break its shield.");
        }
    } else if (store_level) {
        push_message("A store level appears. Approach the shopkeeper to browse.");
    }
}

void Game::scale_enemies_for_depth() {
    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        auto* enemy = dynamic_cast<map::entities::Enemy*>(entity.get());
        if (enemy != nullptr && !enemy->is_boss()) {
            // Boss stats are scaled by tier, not by dungeon depth.
            enemy->scale_for_depth(_depth);
        }
    }
}

std::span<const StoreItem> Game::store_items() const noexcept {
    return _store_items;
}

bool Game::store_item_sold(std::size_t index) const noexcept {
    return index < _store_sold.size() && _store_sold[index];
}

bool Game::buy_store_item(std::size_t index) {
    auto* player = _map.player();

    if (player == nullptr || !_store_open || index >= _store_items.size()) {
        return false;
    }

    const StoreItem& item = _store_items[index];

    if (store_item_sold(index)) {
        push_message("The " + item.name + " is already sold.");
        return false;
    }

    if (player->stats().gold < item.price) {
        push_message("You cannot afford the " + item.name + ".");
        return false;
    }

    const bool owned = player->has_item(item.kind);

    if (!player->can_equip(item.kind)) {
        push_message("Your inventory is full.");
        return false;
    }

    player->stats().gold -= item.price;
    static_cast<void>(player->equip(map::entities::make_item(item.kind)));
    _store_sold[index] = true;

    push_message((owned ? "Upgraded the " : "Bought the ") + item.name + " for " +
                 std::to_string(item.price) + " gold.");
    return true;
}

const map::entities::Shopkeeper* Game::nearby_shopkeeper() const noexcept {
    const auto* player = _map.player();
    if (player == nullptr) {
        return nullptr;
    }

    for (const map::EntityPtr& entity : _map.entity_ptrs()) {
        const auto* shopkeeper = dynamic_cast<const map::entities::Shopkeeper*>(entity.get());
        if (shopkeeper == nullptr) {
            continue;
        }

        const std::size_t dx =
            shopkeeper->x > player->x ? shopkeeper->x - player->x : player->x - shopkeeper->x;
        const std::size_t dy =
            shopkeeper->y > player->y ? shopkeeper->y - player->y : player->y - shopkeeper->y;

        if (dx + dy <= kShopkeeperReach) {
            return shopkeeper;
        }
    }

    return nullptr;
}

map::entities::Shopkeeper* Game::nearby_shopkeeper() noexcept {
    return const_cast<map::entities::Shopkeeper*>(std::as_const(*this).nearby_shopkeeper());
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

    const map::entities::ItemKind selected_kind = chest->contents()[_chest_selection].kind();
    const bool selected_is_currency = selected_kind == map::entities::ItemKind::Coin ||
                                      selected_kind == map::entities::ItemKind::CoinDouble ||
                                      selected_kind == map::entities::ItemKind::Diamond;

    if (!selected_is_currency && !player->can_equip(selected_kind)) {
        push_message("Your inventory is full.");
        return false;
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
        _run_stats.gold_collected += gold;
        push_message("+" + std::to_string(gold) + " gold");
    } else {
        const map::entities::ItemKind kind = item->kind();

        if (!player->equip(*item)) {
            chest->add(std::move(*item));
            push_message("Your inventory is full.");
            return false;
        }

        if (kind == map::entities::ItemKind::Key) {
            ++_run_stats.keys_collected;
        }

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
        _run_stats.gold_collected += gold;
        push_message("+" + std::to_string(gold) + " gold");
        break;
    }
    case map::entities::ItemKind::Key:
        if (!event.player.equip(event.item)) {
            push_message("Your inventory is full.");
            break;
        }
        static_cast<void>(_map.remove(event.item.id()));
        ++_run_stats.keys_collected;
        push_message("Key acquired.");
        break;
    case map::entities::ItemKind::HealthPotion:
        if (!event.player.equip(event.item)) {
            push_message("Your inventory is full.");
            break;
        }
        static_cast<void>(_map.remove(event.item.id()));
        push_message("Health potion acquired.");
        break;
    case map::entities::ItemKind::Sword:
    case map::entities::ItemKind::Greatsword:
    case map::entities::ItemKind::Shield:
    case map::entities::ItemKind::Chainmail:
    case map::entities::ItemKind::Boots: {
        const map::entities::ItemKind kind  = event.item.kind();
        const bool                    owned = event.player.has_item(kind);

        if (!event.player.equip(event.item)) {
            push_message("Your inventory is full.");
            break;
        }

        static_cast<void>(_map.remove(event.item.id()));
        push_message(item_name(kind) + (owned ? " upgraded." : " equipped."));
        break;
    }
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
