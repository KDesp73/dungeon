#include "agent/observation.hpp"

#include "dungeon/map/entities.hpp"
#include "dungeon/scene/game_scene.hpp"
#include "dungeon/scene/pause_scene.hpp"
#include "dungeon/scene/store_scene.hpp"
#include "dungeon/scene/title_scene.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace dungeon::agent {
namespace {

using nlohmann::json;

// The crop sent to Kev is 21x13 around the player. Only loot inside that crop
// is advertised, so the model never sees items that are absent from its map.
constexpr std::size_t kVisibleRadiusX = 10;
constexpr std::size_t kVisibleRadiusY = 6;

const char* item_name(map::entities::ItemKind kind) noexcept {
    using map::entities::ItemKind;
    switch (kind) {
    case ItemKind::Coin:
        return "coin";
    case ItemKind::CoinDouble:
        return "double coin";
    case ItemKind::Key:
        return "key";
    case ItemKind::Chest:
        return "chest";
    case ItemKind::Diamond:
        return "diamond";
    case ItemKind::HealthPotion:
        return "health potion";
    case ItemKind::Sword:
        return "sword";
    case ItemKind::Greatsword:
        return "greatsword";
    case ItemKind::Shield:
        return "shield";
    case ItemKind::Chainmail:
        return "chainmail";
    case ItemKind::Boots:
        return "boots";
    }
    return "item";
}

std::vector<std::string> recent(const std::vector<std::string>& messages,
                                std::size_t count) {
    const std::size_t first = messages.size() > count ? messages.size() - count : 0;
    return {messages.begin() + static_cast<std::ptrdiff_t>(first), messages.end()};
}

std::vector<std::string> crop(const AgentObservation& observation, std::size_t radius_x,
                              std::size_t radius_y) {
    std::vector<std::string> result;
    if (observation.visible.empty()) {
        return result;
    }

    for (std::size_t dy = 0; dy <= radius_y * 2; ++dy) {
        const std::ptrdiff_t signed_y =
            static_cast<std::ptrdiff_t>(observation.player.y) + static_cast<std::ptrdiff_t>(dy) -
            static_cast<std::ptrdiff_t>(radius_y);
        std::string row;
        if (signed_y < 0 || static_cast<std::size_t>(signed_y) >= observation.visible.size()) {
            row.assign(radius_x * 2 + 1, ' ');
            result.push_back(row);
            continue;
        }

        const std::string& source = observation.visible[static_cast<std::size_t>(signed_y)];
        for (std::size_t dx = 0; dx <= radius_x * 2; ++dx) {
            const std::ptrdiff_t signed_x =
                static_cast<std::ptrdiff_t>(observation.player.x) +
                static_cast<std::ptrdiff_t>(dx) - static_cast<std::ptrdiff_t>(radius_x);
            if (signed_x < 0 || static_cast<std::size_t>(signed_x) >= source.size()) {
                row.push_back(' ');
            } else {
                row.push_back(source[static_cast<std::size_t>(signed_x)]);
            }
        }
        result.push_back(row);
    }
    return result;
}

std::string describe_enemy(const AgentObservation& observation) {
    static_cast<void>(observation);
    if (!observation.nearest_enemy) {
        return "none";
    }

    const AgentObservation::EnemyInfo& enemy = *observation.nearest_enemy;
    std::ostringstream out;
    out << enemy.name << ", hp " << enemy.hp << ", attack " << enemy.attack << ", defence "
        << enemy.defence << ", distance " << enemy.distance;
    if (enemy.is_boss) {
        out << ", anchors " << enemy.anchors;
    }
    return out.str();
}

bool in_visible_crop(const AgentObservation& observation, AgentObservation::Cell cell) noexcept {
    const std::size_t dx = cell.x > observation.player.x ? cell.x - observation.player.x
                                                         : observation.player.x - cell.x;
    const std::size_t dy = cell.y > observation.player.y ? cell.y - observation.player.y
                                                         : observation.player.y - cell.y;
    return dx <= kVisibleRadiusX && dy <= kVisibleRadiusY;
}

} // namespace

const char* scene_name(SceneKind kind) noexcept {
    switch (kind) {
    case SceneKind::Title:
        return "title";
    case SceneKind::Game:
        return "game";
    case SceneKind::Store:
        return "store";
    case SceneKind::Pause:
        return "pause";
    case SceneKind::Other:
        return "other";
    }
    return "other";
}

std::size_t manhattan(AgentObservation::Cell lhs, AgentObservation::Cell rhs) noexcept {
    const std::size_t dx = lhs.x > rhs.x ? lhs.x - rhs.x : rhs.x - lhs.x;
    const std::size_t dy = lhs.y > rhs.y ? lhs.y - rhs.y : rhs.y - lhs.y;
    return dx + dy;
}

bool in_bounds(const AgentObservation& observation, std::size_t x, std::size_t y) noexcept {
    if (y >= observation.tiles.size()) {
        return false;
    }
    return x < observation.tiles[y].size();
}

bool walkable(const AgentObservation& observation, std::size_t x, std::size_t y) noexcept {
    if (!in_bounds(observation, x, y)) {
        return false;
    }
    const char tile = observation.tiles[y][x];
    return tile == '.' || tile == '#' || tile == '+';
}

AgentObservation AgentObservation::from(const game::Game& game,
                                        const scene::SceneManager& scenes) {
    using map::entities::Anchor;
    using map::entities::Boss;
    using map::entities::Chest;
    using map::entities::Enemy;
    using map::entities::Fireball;
    using map::entities::Item;
    using map::entities::ItemKind;
    using map::entities::Key;
    using map::entities::Player;

    AgentObservation observation;
    observation.depth            = game.depth();
    observation.store_level      = game.store_level();
    observation.boss_level       = game.boss_level();
    observation.game_over        = game.game_over();
    observation.victory          = game.victory();
    observation.store_open       = game.store_open();
    observation.anchors_remaining = game.anchors_remaining();
    observation.run_stats        = game.run_stats();
    observation.recent_messages  = recent(game.messages(), 6);

    const scene::Scene* top = scenes.top();
    if (dynamic_cast<const scene::TitleScene*>(top) != nullptr) {
        observation.scene = SceneKind::Title;
    } else if (dynamic_cast<const scene::StoreScene*>(top) != nullptr) {
        observation.scene = SceneKind::Store;
    } else if (dynamic_cast<const scene::PauseScene*>(top) != nullptr) {
        observation.scene = SceneKind::Pause;
    } else if (dynamic_cast<const scene::GameScene*>(top) != nullptr) {
        observation.scene = SceneKind::Game;
    }

    const auto* store_scene = scenes.find<scene::StoreScene>();
    if (store_scene != nullptr) {
        observation.store_selection = store_scene->selected();
    }

    const map::Map& map = game.map();
    observation.tiles.reserve(map.height());
    for (std::size_t y = 0; y < map.height(); ++y) {
        std::string row;
        row.reserve(map.width());
        for (std::size_t x = 0; x < map.width(); ++x) {
            row.push_back(map::glyph(map.at(x, y)));
        }
        observation.tiles.push_back(row);
    }
    observation.visible = observation.tiles;

    const auto place_glyph = [&observation](std::size_t x, std::size_t y, char glyph) {
        if (in_bounds(observation, x, y)) {
            observation.visible[y][x] = glyph;
        }
    };

    const auto* player = game.player();
    if (player != nullptr) {
        const auto stats       = player->stats();
        observation.player     = {player->x, player->y};
        observation.hp         = stats.hp;
        observation.max_hp     = map::entities::max_hp(stats);
        observation.gold       = stats.gold;
        observation.attack     = stats.attack;
        observation.defence    = stats.defence;
        observation.stamina    = stats.stamina;
        observation.has_key    = player->has_key();
        observation.potions    = static_cast<std::size_t>(std::count_if(
            player->inventory().begin(), player->inventory().end(),
            [](const Item& item) { return item.kind() == ItemKind::HealthPotion; }));
        place_glyph(player->x, player->y, player->tile());
    }

    observation.exit_cell     = map.exit();
    observation.exit_unlocked = map.exit_unlocked();
    place_glyph(observation.exit_cell.x, observation.exit_cell.y,
                observation.exit_unlocked ? '[' : ']');

    for (const map::EntityPtr& entity : map.entity_ptrs()) {
        if (entity.get() == player) {
            continue;
        }

        place_glyph(entity->x, entity->y, entity->tile());

        if (const auto* key = dynamic_cast<const Key*>(entity.get())) {
            observation.key_on_map = true;
            observation.key_cell   = {key->x, key->y};
        } else if (const auto* item = dynamic_cast<const Item*>(entity.get())) {
            const bool empty_chest = item->kind() == ItemKind::Chest &&
                                     dynamic_cast<const Chest*>(item)->empty();
            if (item->kind() != ItemKind::Key && !empty_chest) {
                const AgentObservation::Cell cell{item->x, item->y};
                observation.loot.push_back(AgentObservation::LootInfo{
                    item_name(item->kind()), item->tile(), item->gold(), cell,
                    manhattan(observation.player, cell)});
            }
        } else if (const auto* enemy = dynamic_cast<const Enemy*>(entity.get())) {
            if (enemy->stats().hp > 0) {
                observation.obstacles.push_back({enemy->x, enemy->y, enemy->tile()});
            }
        } else if (const auto* anchor = dynamic_cast<const Anchor*>(entity.get())) {
            if (anchor->alive()) {
                observation.obstacles.push_back({anchor->x, anchor->y, anchor->tile()});
            }
        } else if (const auto* fireball = dynamic_cast<const Fireball*>(entity.get())) {
            if (!fireball->expired()) {
                observation.obstacles.push_back({fireball->x, fireball->y, fireball->tile()});
            }
        }
    }

    if (observation.key_on_map) {
        observation.distance_to_key = manhattan(observation.player, observation.key_cell);
    }

    if (const auto* enemy = game.nearest_enemy()) {
        const auto stats           = enemy->stats();
        const bool is_boss         = enemy->is_boss();
        const auto* boss           = is_boss ? dynamic_cast<const Boss*>(enemy) : nullptr;
        observation.nearest_enemy  = AgentObservation::EnemyInfo{
            std::string(enemy->name()), stats.hp, stats.attack, stats.defence,
            manhattan(observation.player, {enemy->x, enemy->y}), {enemy->x, enemy->y}, is_boss,
            boss != nullptr ? boss->anchors_remaining() : 0};
    }

    if (const auto* chest = game.nearby_chest()) {
        observation.chest_nearby    = true;
        observation.chest_selection = game.chest_selection();
        for (const Item& item : chest->contents()) {
            observation.chest_items.push_back(item_name(item.kind()));
        }
    }
    observation.shopkeeper_nearby = game.nearby_shopkeeper() != nullptr;

    for (std::size_t index = 0; index < game.store_items().size(); ++index) {
        const auto& item = game.store_items()[index];
        const bool sold = game.store_item_sold(index);
        const bool affordable = !sold && observation.gold >= item.price;
        observation.store_items.push_back(AgentObservation::StoreOption{
            item.name, item.description, item.price, sold, affordable});
    }

    return observation;
}

std::string AgentObservation::to_json() const {
    json state;
    state["game"]        = "dungeon";
    state["depth"]       = depth;
    state["scene"]       = scene_name(scene);
    state["store_level"] = store_level;
    state["boss_level"]  = boss_level;
    state["game_over"]   = game_over;
    state["victory"]     = victory;
    state["player"]      = {{"hp", hp},
                            {"max_hp", max_hp},
                            {"gold", gold},
                            {"attack", attack},
                            {"defence", defence},
                            {"potions", potions},
                            {"has_key", has_key}};
    state["progress"]    = {{"exit_unlocked", exit_unlocked},
                            {"distance_to_key", distance_to_key},
                            {"nearest_enemy", describe_enemy(*this)},
                            {"anchors_remaining", anchors_remaining}};
    state["visible_map"] = crop(*this, kVisibleRadiusX, kVisibleRadiusY);
    state["messages"]    = recent_messages;

    if (nearest_enemy) {
        state["progress"]["enemy_distance"] = nearest_enemy->distance;
    }
    state["progress"]["key_close"] =
        key_on_map && !has_key && distance_to_key <= kNearKeyDistance;

    // Advertise collectible loot with its gold value so Kev can weigh detouring
    // for coins and equipment against reaching the key or the exit.
    json loot_items = json::array();
    std::size_t loot_gold = 0;
    const LootInfo* nearest = nullptr;
    for (const LootInfo& item : loot) {
        if (!in_visible_crop(*this, item.cell)) {
            continue;
        }
        loot_items.push_back({{"name", item.name},
                              {"glyph", std::string(1, item.glyph)},
                              {"gold", item.gold},
                              {"x", item.cell.x},
                              {"y", item.cell.y},
                              {"distance", item.distance}});
        loot_gold += static_cast<std::size_t>(item.gold > 0 ? item.gold : 0);
        if (nearest == nullptr || item.distance < nearest->distance ||
            (item.distance == nearest->distance && item.gold > nearest->gold)) {
            nearest = &item;
        }
    }
    state["loot"]                     = loot_items;
    state["progress"]["loot_visible"] = loot_items.size();
    state["progress"]["loot_gold"]    = loot_gold;
    if (nearest != nullptr) {
        std::ostringstream description;
        description << nearest->name << ", gold " << nearest->gold << ", distance "
                    << nearest->distance;
        state["progress"]["nearest_loot"] = description.str();
    }

    if (scene == SceneKind::Store || store_open) {
        json items = json::array();
        for (std::size_t index = 0; index < store_items.size(); ++index) {
            const StoreOption& item = store_items[index];
            items.push_back({{"index", index},
                             {"name", item.name},
                             {"description", item.description},
                             {"price", item.price},
                             {"sold", item.sold},
                             {"affordable", item.affordable}});
        }
        state["store"] = {{"selected", store_selection}, {"items", items}};
    }

    if (chest_nearby) {
        state["chest"] = {{"selected", chest_selection}, {"items", chest_items}};
    }

    return state.dump();
}

} // namespace dungeon::agent
