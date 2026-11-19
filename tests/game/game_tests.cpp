#include "test_harness.hpp"

#include "dungeon/game.hpp"
#include "dungeon/map/map.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using dungeon::game::ActionEvent;
using dungeon::game::ActionTrigger;
using dungeon::game::Direction;
using dungeon::game::direction_from_key;
using dungeon::game::Game;
using dungeon::map::DoorSide;
using dungeon::map::MapSpec;
using dungeon::map::entities::Anchor;
using dungeon::map::entities::Boss;
using dungeon::map::entities::EnemyKind;
using dungeon::map::entities::Fireball;
using dungeon::map::entities::ItemKind;

namespace {

MapSpec make_test_spec() {
    MapSpec spec;
    spec.width       = 20;
    spec.height      = 10;
    spec.rooms       = {{2, 2, 6, 5}, {12, 2, 6, 5}};
    spec.connections = {{0, 1, DoorSide::Right, DoorSide::Left}};
    spec.entrance    = {3, 3};
    spec.exit        = {15, 3};
    return spec;
}

struct RecordedAction {
    ActionTrigger trigger;
    char          tile;
    std::size_t   x;
    std::size_t   y;
};

void record_actions(Game& game, std::vector<RecordedAction>& actions) {
    game.set_action_hook([&actions](const ActionEvent& event) {
        actions.push_back(
            RecordedAction{event.trigger, event.item.tile(), event.item.x, event.item.y});
    });
}

bool has_action(const std::vector<RecordedAction>& actions, ActionTrigger trigger, char tile,
                std::size_t x, std::size_t y) {
    for (const RecordedAction& action : actions) {
        if (action.trigger == trigger && action.tile == tile && action.x == x && action.y == y) {
            return true;
        }
    }
    return false;
}

std::size_t count_actions(const std::vector<RecordedAction>& actions, ActionTrigger trigger) {
    std::size_t count = 0;
    for (const RecordedAction& action : actions) {
        if (action.trigger == trigger) {
            ++count;
        }
    }
    return count;
}

std::size_t count_fireballs(const Game& game) {
    std::size_t count = 0;
    for (const dungeon::map::EntityPtr& entity : game.map().entity_ptrs()) {
        if (dynamic_cast<const Fireball*>(entity.get()) != nullptr) {
            ++count;
        }
    }
    return count;
}

Anchor* first_anchor(Game& game) {
    for (const dungeon::map::EntityPtr& entity : game.map().entity_ptrs()) {
        auto* anchor = dynamic_cast<Anchor*>(entity.get());
        if (anchor != nullptr && anchor->alive()) {
            return anchor;
        }
    }
    return nullptr;
}

void destroy_all_anchors(Game& game) {
    while (game.anchors_remaining() > 0) {
        Anchor* anchor = first_anchor(game);
        REQUIRE(anchor != nullptr);
        game.player()->place(anchor->x, anchor->y - 1);
        CHECK(game.move(Direction::Down));
    }
}

void strike_boss_until_dead(Game& game) {
    auto* boss = game.boss();
    REQUIRE(boss != nullptr);

    // A single huge hit keeps these tests focused on the boss lifecycle rather
    // than on how many melee strikes its tier needs.
    game.player()->stats().attack = 1000;
    game.player()->place(boss->x, boss->y + 1);

    for (int i = 0; i < 10 && game.boss() != nullptr; ++i) {
        CHECK(game.move(Direction::Up));
    }
}

} // namespace

DUNGEON_TEST(game, player_is_accessible_without_searching) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Coin, 4, 3}};

    Game game(spec);

    REQUIRE(game.player() != nullptr);
    CHECK_EQ(game.player()->x, spec.entrance.x);
    CHECK_EQ(game.player()->y, spec.entrance.y);
    CHECK(game.map().player() == game.player());
}

DUNGEON_TEST(game, stepping_onto_an_item_triggers_a_step_action) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Coin, 4, 3}};

    Game                        game(spec);
    std::vector<RecordedAction> actions;
    record_actions(game, actions);

    CHECK(game.move(Direction::Right));

    REQUIRE(game.player() != nullptr);
    CHECK_EQ(game.player()->x, 4U);
    CHECK_EQ(game.player()->y, 3U);
    CHECK(has_action(actions, ActionTrigger::Step, '$', 4, 3));
    CHECK_EQ(count_actions(actions, ActionTrigger::Step), 1U);
}

DUNGEON_TEST(game, stepping_on_a_coin_collects_it_and_adds_gold) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Coin, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    const auto before = game.player()->stats();

    CHECK(game.move(Direction::Right));

    const auto after = game.player()->stats();
    CHECK_EQ(after.gold, before.gold + 1);
    CHECK_EQ(game.run_stats().gold_collected, 1);
    CHECK_EQ(after.level, before.level);
    CHECK_EQ(after.hp, before.hp);
    CHECK_EQ(after.attack, before.attack);
    CHECK_EQ(after.defence, before.defence);
    CHECK_EQ(after.stamina, before.stamina);
    CHECK_EQ(game.map().entity_ptrs().size(), 1U);
}

DUNGEON_TEST(game, stepping_on_a_double_coin_adds_two_gold) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::CoinDouble, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    const auto before = game.player()->stats();

    CHECK(game.move(Direction::Right));

    const auto after = game.player()->stats();
    CHECK_EQ(after.gold, before.gold + 2);
    CHECK_EQ(game.run_stats().gold_collected, 2);
    CHECK_EQ(after.level, before.level);
    CHECK_EQ(after.hp, before.hp);
    CHECK_EQ(after.attack, before.attack);
    CHECK_EQ(after.defence, before.defence);
    CHECK_EQ(after.stamina, before.stamina);
    CHECK_EQ(game.map().entity_ptrs().size(), 1U);
}

DUNGEON_TEST(game, stepping_on_the_key_equips_it_into_the_inventory) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Key, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    CHECK(!game.player()->has_key());
    CHECK(game.player()->inventory().empty());
    const auto before = game.player()->stats();

    CHECK(game.move(Direction::Right));

    const auto after = game.player()->stats();
    CHECK_EQ(after.level, before.level);
    CHECK_EQ(after.hp, before.hp);
    CHECK_EQ(after.attack, before.attack);
    CHECK_EQ(after.defence, before.defence);
    CHECK_EQ(after.stamina, before.stamina);
    CHECK_EQ(after.gold, before.gold);
    CHECK(game.player()->has_key());
    CHECK_EQ(game.run_stats().keys_collected, 1U);
    REQUIRE(!game.player()->inventory().empty());
    CHECK_EQ(game.player()->inventory().front().tile(), 'k');
    CHECK_EQ(game.map().entity_ptrs().size(), 1U);
}

DUNGEON_TEST(game, the_exit_unlocks_after_the_key_is_equipped) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Key, 4, 3}};

    Game game(spec);

    CHECK(!game.map().exit_unlocked());

    CHECK(game.move(Direction::Right));

    CHECK(game.player()->has_key());
    CHECK(game.map().exit_unlocked());
}

DUNGEON_TEST(game, stepping_on_a_sword_equips_it_and_raises_attack) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Sword, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    const int attack_before = game.player()->stats().attack;

    CHECK(game.move(Direction::Right));

    CHECK_EQ(game.player()->stats().attack, attack_before + 2);
    CHECK(game.player()->has_item(ItemKind::Sword));
    CHECK_EQ(game.map().entity_ptrs().size(), 1U);
}

DUNGEON_TEST(game, stepping_on_owned_equipment_upgrades_it_instead_of_duplicating_it) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Sword, 4, 3}, {ItemKind::Sword, 5, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    const int attack_before = game.player()->stats().attack;

    CHECK(game.move(Direction::Right));
    REQUIRE(game.player()->has_item(ItemKind::Sword));
    CHECK_EQ(game.player()->inventory().size(), 1U);

    CHECK(game.move(Direction::Right));

    CHECK_EQ(game.player()->inventory().size(), 1U);
    CHECK_EQ(game.player()->stats().attack, attack_before + 4);
    REQUIRE(!game.player()->inventory().empty());
    CHECK_EQ(game.player()->inventory().front().level(), 2U);
    REQUIRE(!game.messages().empty());
    CHECK_EQ(game.messages().back(), std::string("sword upgraded."));
}

DUNGEON_TEST(game, stepping_on_a_health_potion_stores_it_without_healing) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::HealthPotion, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    game.player()->stats().hp = 50;

    CHECK(game.move(Direction::Right));

    CHECK_EQ(game.player()->stats().hp, 50);
    CHECK(game.player()->has_item(ItemKind::HealthPotion));
    REQUIRE(!game.player()->inventory().empty());
    CHECK_EQ(game.player()->inventory().front().tile(), '!');
    REQUIRE(!game.messages().empty());
    CHECK_EQ(game.messages().back(), std::string("Health potion acquired."));
}

DUNGEON_TEST(game, using_a_health_potion_heals_a_random_amount_and_consumes_it) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::HealthPotion, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    game.player()->stats().hp = 50;
    REQUIRE(game.move(Direction::Right));
    REQUIRE(game.player()->has_item(ItemKind::HealthPotion));

    CHECK(game.use_health_potion());

    const int hp = game.player()->stats().hp;
    CHECK(hp >= 50 + dungeon::map::entities::HEALTH_POTION_HEAL_MIN);
    CHECK(hp <= 50 + dungeon::map::entities::HEALTH_POTION_HEAL_MAX);
    CHECK(!game.player()->has_item(ItemKind::HealthPotion));
    CHECK_EQ(game.run_stats().potions_used, 1U);
    REQUIRE(!game.messages().empty());
    CHECK(game.messages().back().find("recover") != std::string::npos);
}

DUNGEON_TEST(game, a_health_potion_is_not_consumed_at_full_health) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::HealthPotion, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    REQUIRE(game.move(Direction::Right));
    REQUIRE(game.player()->has_item(ItemKind::HealthPotion));

    CHECK(!game.use_health_potion());

    CHECK_EQ(game.player()->stats().hp, dungeon::map::entities::PLAYER_MAX_HP);
    CHECK(game.player()->has_item(ItemKind::HealthPotion));
    REQUIRE(!game.messages().empty());
    CHECK_EQ(game.messages().back(), std::string("You are already at full health."));
}

DUNGEON_TEST(game, using_a_health_potion_without_one_does_nothing) {
    Game game(make_test_spec());
    REQUIRE(game.player() != nullptr);

    CHECK(!game.use_health_potion());

    REQUIRE(!game.messages().empty());
    CHECK_EQ(game.messages().back(), std::string("You have no health potions."));
}

DUNGEON_TEST(game, the_inventory_holds_at_most_ten_items) {
    MapSpec spec = make_test_spec();

    for (int i = 0; i < 11; ++i) {
        spec.items.push_back({ItemKind::HealthPotion, 4, 3});
    }

    Game game(spec);
    REQUIRE(game.player() != nullptr);

    CHECK(game.move(Direction::Right));

    CHECK_EQ(game.player()->inventory().size(),
             dungeon::map::entities::PLAYER_INVENTORY_SLOTS);
    CHECK(game.player()->inventory_full());

    std::size_t potions_on_map = 0;
    for (const dungeon::map::EntityPtr& entity : game.map().entity_ptrs()) {
        if (dynamic_cast<const dungeon::map::entities::HealthPotion*>(entity.get()) != nullptr) {
            ++potions_on_map;
        }
    }

    CHECK_EQ(potions_on_map, 1U);
    REQUIRE(!game.messages().empty());
    CHECK_EQ(game.messages().back(), std::string("Your inventory is full."));
}

DUNGEON_TEST(game, a_full_inventory_blocks_buying_new_equipment) {
    Game game(80, 30);
    REQUIRE(game.player() != nullptr);

    for (std::size_t i = 0; i < dungeon::map::entities::PLAYER_INVENTORY_SLOTS; ++i) {
        REQUIRE(game.player()->equip(dungeon::map::entities::make_item(ItemKind::HealthPotion)));
    }

    game.player()->stats().gold = 1000;

    for (int i = 0; i < 4; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    game.open_store();
    REQUIRE(game.store_open());

    const int gold_before = game.player()->stats().gold;

    CHECK(!game.buy_store_item(0));
    CHECK_EQ(game.player()->stats().gold, gold_before);
    CHECK(!game.store_item_sold(0));
    REQUIRE(!game.messages().empty());
    CHECK_EQ(game.messages().back(), std::string("Your inventory is full."));
}

DUNGEON_TEST(game, a_full_inventory_still_allows_upgrading_owned_equipment) {
    Game game(80, 30);
    REQUIRE(game.player() != nullptr);

    REQUIRE(game.player()->equip(dungeon::map::entities::make_item(ItemKind::Sword)));

    for (std::size_t i = 1; i < dungeon::map::entities::PLAYER_INVENTORY_SLOTS; ++i) {
        REQUIRE(game.player()->equip(dungeon::map::entities::make_item(ItemKind::HealthPotion)));
    }

    REQUIRE(game.player()->inventory_full());
    game.player()->stats().gold = 1000;

    for (int i = 0; i < 4; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    game.open_store();

    const std::size_t inventory_size = game.player()->inventory().size();
    const int         attack_before  = game.player()->stats().attack;

    CHECK(game.buy_store_item(0));
    CHECK_EQ(game.player()->inventory().size(), inventory_size);
    CHECK_EQ(game.player()->stats().attack, attack_before + 2);
}

DUNGEON_TEST(game, stepping_on_the_unlocked_exit_generates_a_new_map_and_keeps_player_state) {
    MapSpec spec;
    spec.width    = 80;
    spec.height   = 30;
    spec.rooms    = {{1, 1, 78, 28}};
    spec.entrance = {3, 3};
    spec.exit     = {6, 3};
    spec.items    = {{ItemKind::Key, 4, 3}, {ItemKind::Coin, 5, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);

    CHECK(game.move(Direction::Right)); // key
    CHECK(game.move(Direction::Right)); // coin

    const auto stats_before     = game.player()->stats();
    const auto inventory_before = game.player()->inventory();
    REQUIRE(game.player()->has_key());

    CHECK(game.move(Direction::Right)); // exit

    REQUIRE(game.player() != nullptr);
    const auto stats_after = game.player()->stats();

    CHECK_EQ(stats_after.level, stats_before.level);
    CHECK_EQ(stats_after.hp, stats_before.hp);
    CHECK_EQ(stats_after.attack, stats_before.attack);
    CHECK_EQ(stats_after.defence, stats_before.defence);
    CHECK_EQ(stats_after.stamina, stats_before.stamina);
    CHECK_EQ(stats_after.gold, stats_before.gold);

    CHECK_EQ(game.player()->inventory().size(), inventory_before.size() - 1);
    CHECK(!game.player()->has_key());
    CHECK(game.player()->inventory().empty());
    CHECK(!game.map().exit_unlocked());

    CHECK_EQ(game.player()->x, game.map().entrance().x);
    CHECK_EQ(game.player()->y, game.map().entrance().y);
    CHECK_EQ(game.map().width(), 80U);
    CHECK_EQ(game.map().height(), 30U);
}

DUNGEON_TEST(game, stepping_on_a_locked_exit_does_not_generate_a_new_map) {
    MapSpec spec;
    spec.width    = 40;
    spec.height   = 20;
    spec.rooms    = {{1, 1, 38, 18}};
    spec.entrance = {3, 3};
    spec.exit     = {6, 3};

    Game game(spec);
    REQUIRE(game.player() != nullptr);

    CHECK(game.move(Direction::Right));
    CHECK(game.move(Direction::Right));
    CHECK(game.move(Direction::Right)); // locked exit

    REQUIRE(game.player() != nullptr);
    CHECK_EQ(game.player()->x, spec.exit.x);
    CHECK_EQ(game.player()->y, spec.exit.y);
    CHECK_EQ(game.map().entrance().x, spec.entrance.x);
    CHECK_EQ(game.map().entrance().y, spec.entrance.y);
    CHECK(!game.map().exit_unlocked());
}

DUNGEON_TEST(game, an_interactable_one_square_away_triggers_a_nearby_action) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Chest, 5, 3}};

    Game                        game(spec);
    std::vector<RecordedAction> actions;
    record_actions(game, actions);

    CHECK(game.move(Direction::Right));

    CHECK(has_action(actions, ActionTrigger::Nearby, '?', 5, 3));
    CHECK_EQ(count_actions(actions, ActionTrigger::Step), 0U);
}

DUNGEON_TEST(game, a_nearby_chest_exposes_its_contents) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Chest,
                     5,
                     3,
                     {ItemKind::Coin, ItemKind::CoinDouble}}};

    Game game(spec);
    REQUIRE(game.move(Direction::Right));

    const auto* chest = game.nearby_chest();
    REQUIRE(chest != nullptr);
    REQUIRE(chest->contents().size() == 2U);
    CHECK(chest->contents()[0].kind() == ItemKind::Coin);
    CHECK(chest->contents()[1].kind() == ItemKind::CoinDouble);
}

DUNGEON_TEST(game, the_selected_chest_item_can_be_taken) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Chest,
                     5,
                     3,
                     {ItemKind::Coin, ItemKind::CoinDouble}}};

    Game game(spec);
    REQUIRE(game.move(Direction::Right));
    REQUIRE(game.player() != nullptr);

    const int gold_before = game.player()->stats().gold;

    game.cycle_chest_selection(1);
    CHECK_EQ(game.chest_selection(), 1U);
    CHECK(game.take_selected_chest_item());

    CHECK_EQ(game.player()->stats().gold, gold_before + 2);
    REQUIRE(game.nearby_chest() != nullptr);
    REQUIRE(game.nearby_chest()->contents().size() == 1U);
    CHECK_EQ(game.chest_selection(), 0U);
}

DUNGEON_TEST(game, taking_a_key_from_a_chest_equips_it) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Chest, 5, 3, {ItemKind::Key}}};

    Game game(spec);
    REQUIRE(game.move(Direction::Right));
    REQUIRE(game.player() != nullptr);

    CHECK(!game.player()->has_key());
    CHECK(game.take_selected_chest_item());
    CHECK(game.player()->has_key());
    CHECK_EQ(game.run_stats().keys_collected, 1U);
    REQUIRE(game.nearby_chest() != nullptr);
    CHECK(game.nearby_chest()->empty());
}

DUNGEON_TEST(game, taking_a_health_potion_from_a_chest_stores_it) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Chest, 5, 3, {ItemKind::HealthPotion}}};

    Game game(spec);
    REQUIRE(game.move(Direction::Right));
    REQUIRE(game.player() != nullptr);

    CHECK(!game.player()->has_item(ItemKind::HealthPotion));
    CHECK(game.take_selected_chest_item());
    CHECK(game.player()->has_item(ItemKind::HealthPotion));
    REQUIRE(game.nearby_chest() != nullptr);
    CHECK(game.nearby_chest()->empty());
}

DUNGEON_TEST(game, taking_from_an_empty_chest_fails_and_reports_it) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Chest, 5, 3}};

    Game game(spec);
    REQUIRE(game.move(Direction::Right));

    CHECK(!game.take_selected_chest_item());
    REQUIRE(!game.messages().empty());
    CHECK_EQ(game.messages().back(), std::string("The chest is empty."));
}

DUNGEON_TEST(game, a_collected_coin_adds_a_message) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Coin, 4, 3}};

    Game game(spec);
    REQUIRE(game.move(Direction::Right));

    CHECK(std::find(game.messages().begin(), game.messages().end(), std::string("+1 gold")) !=
          game.messages().end());
}

DUNGEON_TEST(game, a_hook_can_remove_the_item_it_receives) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Chest, 4, 3}};

    Game game(spec);
    game.set_action_hook([&game](const ActionEvent& event) {
        static_cast<void>(game.map().remove(event.item.id()));
    });

    CHECK(game.move(Direction::Right));
    CHECK_EQ(game.map().entity_ptrs().size(), 1U);
    REQUIRE(game.player() != nullptr);
    CHECK_EQ(game.player()->x, 4U);
    CHECK_EQ(game.player()->y, 3U);
}

DUNGEON_TEST(game, equipable_items_do_not_trigger_nearby_actions) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Coin, 4, 3}, {ItemKind::Coin, 4, 4}};

    Game                        game(spec);
    std::vector<RecordedAction> actions;
    record_actions(game, actions);

    CHECK(game.move(Direction::Right));

    CHECK(has_action(actions, ActionTrigger::Step, '$', 4, 3));
    CHECK(!has_action(actions, ActionTrigger::Nearby, '$', 4, 4));
    CHECK_EQ(count_actions(actions, ActionTrigger::Nearby), 0U);
}

DUNGEON_TEST(game, a_blocked_move_keeps_the_player_and_triggers_nothing) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Chest, 4, 3}};

    Game                        game(spec);
    std::vector<RecordedAction> actions;
    record_actions(game, actions);

    CHECK(!game.move(Direction::Up));

    REQUIRE(game.player() != nullptr);
    CHECK_EQ(game.player()->x, spec.entrance.x);
    CHECK_EQ(game.player()->y, spec.entrance.y);
    CHECK(actions.empty());
}

DUNGEON_TEST(game, moving_without_a_player_fails) {
    Game game;

    CHECK(game.player() == nullptr);
    CHECK(!game.move(Direction::Up));
}

DUNGEON_TEST(game, an_adjacent_enemy_attacks_the_player) {
    MapSpec spec = make_test_spec();
    spec.enemies = {{EnemyKind::Normal, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    const int hp_before = game.player()->stats().hp;

    game.update(2.0);

    CHECK_EQ(game.player()->stats().hp, hp_before - 5);
    CHECK(!game.game_over());
    REQUIRE(!game.messages().empty());
    CHECK_EQ(game.messages().back(), std::string("The Goblin hits you for 5."));
}

DUNGEON_TEST(game, a_distant_enemy_does_not_attack_until_it_reaches_the_player) {
    MapSpec spec = make_test_spec();
    spec.enemies = {{EnemyKind::Normal, 15, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    const int hp_before = game.player()->stats().hp;

    game.update(0.1);

    CHECK_EQ(game.player()->stats().hp, hp_before);
}

DUNGEON_TEST(game, an_alerted_enemy_chases_the_player) {
    MapSpec spec;
    spec.width    = 24;
    spec.height   = 12;
    spec.rooms    = {{1, 1, 22, 10}};
    spec.entrance = {3, 5};
    spec.exit     = {20, 8};
    spec.enemies  = {{EnemyKind::Fast, 9, 5}};

    Game game(spec);
    const auto* enemy = game.nearest_enemy();
    REQUIRE(enemy != nullptr);
    CHECK_EQ(enemy->x, 9U);

    game.update(0.3);

    CHECK(game.nearest_enemy() != nullptr);
    CHECK_EQ(game.nearest_enemy()->x, 8U);
    CHECK_EQ(game.nearest_enemy()->y, 5U);
    CHECK(game.nearest_enemy()->alerted());
    CHECK_EQ(game.player()->stats().hp, 100);
}

DUNGEON_TEST(game, moving_into_an_enemy_attacks_it_and_drops_variable_coins) {
    MapSpec spec = make_test_spec();
    spec.enemies = {{EnemyKind::Fast, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    const int gold_before = game.player()->stats().gold;

    CHECK(game.move(Direction::Right));

    CHECK(game.nearest_enemy() == nullptr);
    CHECK_EQ(game.run_stats().enemies_killed, 1U);
    CHECK_EQ(game.player()->x, spec.entrance.x);
    CHECK_EQ(game.player()->y, spec.entrance.y);
    REQUIRE(!game.messages().empty());
    CHECK(game.messages().back().find("dies and drops") != std::string::npos);

    std::size_t coins = 0;
    for (const dungeon::map::EntityPtr& entity : game.map().entity_ptrs()) {
        if (dynamic_cast<const dungeon::map::entities::Coin*>(entity.get()) != nullptr) {
            ++coins;
        }
    }

    CHECK(coins >= 1U);
    CHECK(coins <= 2U);

    CHECK(game.move(Direction::Right));
    CHECK_EQ(game.player()->x, 4U);
    CHECK_EQ(game.player()->y, 3U);
    CHECK(game.player()->stats().gold >= gold_before + 1);
    CHECK(game.player()->stats().gold <= gold_before + 2);
    CHECK(game.run_stats().gold_collected >= 1);
    CHECK(game.run_stats().gold_collected <= 2);
}

DUNGEON_TEST(game, a_brute_drops_more_coins_than_a_rat) {
    MapSpec spec = make_test_spec();
    spec.enemies = {{EnemyKind::Brute, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);

    for (int hit = 0; hit < 20 && game.nearest_enemy() != nullptr; ++hit) {
        CHECK(game.move(Direction::Right));
    }

    CHECK(game.nearest_enemy() == nullptr);

    std::size_t coins = 0;
    for (const dungeon::map::EntityPtr& entity : game.map().entity_ptrs()) {
        if (dynamic_cast<const dungeon::map::entities::Coin*>(entity.get()) != nullptr) {
            ++coins;
        }
    }

    CHECK(coins >= 4U);
    CHECK(coins <= 8U);
}

DUNGEON_TEST(game, the_player_dies_when_an_enemy_reduces_health_to_zero) {
    MapSpec spec;
    spec.width    = 40;
    spec.height   = 24;
    spec.rooms    = {{1, 1, 38, 22}};
    spec.entrance = {3, 3};
    spec.exit     = {30, 12};
    spec.enemies  = {{EnemyKind::Brute, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    game.player()->stats().hp = 1;

    game.update(2.0);

    CHECK(game.game_over());
    CHECK_EQ(game.player()->stats().hp, 0);
    CHECK(!game.move(Direction::Right));

    game.restart();

    CHECK(!game.game_over());
    REQUIRE(game.player() != nullptr);
    CHECK(game.player()->alive());
    CHECK_EQ(game.player()->stats().hp, 100);
}

DUNGEON_TEST(game, restarting_clears_the_run_stats) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Coin, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);
    REQUIRE(game.move(Direction::Right));

    CHECK_EQ(game.run_stats().gold_collected, 1);

    game.restart();

    CHECK_EQ(game.depth(), 1U);
    CHECK_EQ(game.run_stats().gold_collected, 0);
    CHECK_EQ(game.run_stats().enemies_killed, 0U);
    CHECK_EQ(game.run_stats().dungeons_completed, 0U);
    CHECK_EQ(game.run_stats().potions_used, 0U);
    CHECK_EQ(game.run_stats().keys_collected, 0U);
    CHECK_EQ(game.run_stats().bosses_defeated, 0U);
}

DUNGEON_TEST(game, descending_every_five_dungeons_levels_up_and_builds_a_store_map) {
    Game game(80, 30);

    REQUIRE(game.player() != nullptr);
    CHECK_EQ(game.depth(), 1U);
    CHECK(!game.store_level());
    CHECK(!game.store_open());

    const auto start = game.player()->stats();

    for (int i = 0; i < 4; ++i) {
        game.descend();
    }

    CHECK_EQ(game.depth(), 5U);
    CHECK_EQ(game.run_stats().dungeons_completed, 4U);
    CHECK(game.store_level());
    CHECK(!game.store_open());
    CHECK_EQ(game.player()->stats().level, start.level + 1);
    CHECK_EQ(game.player()->stats().attack,
             start.attack + dungeon::map::entities::LEVEL_UP_ATTACK_BONUS);
    CHECK_EQ(game.player()->stats().defence,
             start.defence + dungeon::map::entities::LEVEL_UP_DEFENCE_BONUS);
    CHECK_EQ(game.player()->stats().stamina,
             start.stamina + dungeon::map::entities::LEVEL_UP_STAMINA_BONUS);
    CHECK_EQ(game.player()->stats().hp, dungeon::map::entities::max_hp(game.player()->stats()));

    CHECK(!game.player()->has_key());
    CHECK(game.map().exit_unlocked());

    std::size_t shopkeepers = 0;
    std::size_t stands      = 0;
    std::size_t enemies     = 0;

    for (const dungeon::map::EntityPtr& entity : game.map().entity_ptrs()) {
        if (dynamic_cast<const dungeon::map::entities::Shopkeeper*>(entity.get()) != nullptr) {
            ++shopkeepers;
        }
        if (dynamic_cast<const dungeon::map::entities::StoreStand*>(entity.get()) != nullptr) {
            ++stands;
        }
        if (dynamic_cast<const dungeon::map::entities::Enemy*>(entity.get()) != nullptr) {
            ++enemies;
        }
    }

    CHECK_EQ(shopkeepers, 1U);
    CHECK_EQ(stands, 1U);
    CHECK_EQ(enemies, 0U);
}

DUNGEON_TEST(game, the_shop_menu_opens_only_near_the_shopkeeper) {
    Game game(80, 30);
    REQUIRE(game.player() != nullptr);

    for (int i = 0; i < 4; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    CHECK(game.nearby_shopkeeper() == nullptr);

    const dungeon::map::entities::Shopkeeper* shopkeeper = nullptr;
    for (const dungeon::map::EntityPtr& entity : game.map().entity_ptrs()) {
        if (const auto* candidate =
                dynamic_cast<const dungeon::map::entities::Shopkeeper*>(entity.get())) {
            shopkeeper = candidate;
        }
    }

    REQUIRE(shopkeeper != nullptr);
    game.player()->place(shopkeeper->x, shopkeeper->y + dungeon::game::kShopkeeperReach);

    REQUIRE(game.nearby_shopkeeper() != nullptr);
    CHECK(!game.store_open());

    game.open_store();
    CHECK(game.store_open());

    game.close_store();
    CHECK(!game.store_open());
}

DUNGEON_TEST(game, stepping_into_the_open_store_exit_continues_to_the_next_dungeon) {
    Game game(80, 30);
    REQUIRE(game.player() != nullptr);

    for (int i = 0; i < 4; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    const dungeon::map::Point exit = game.map().exit();
    game.player()->place(exit.x, exit.y + 1);

    CHECK(game.move(Direction::Up));

    CHECK_EQ(game.depth(), 6U);
    CHECK(!game.store_level());
    CHECK(!game.store_open());
    CHECK(!game.player()->has_key());
    CHECK(!game.map().exit_unlocked());
    REQUIRE(game.player() != nullptr);
    CHECK_EQ(game.player()->x, game.map().entrance().x);
    CHECK_EQ(game.player()->y, game.map().entrance().y);
}

DUNGEON_TEST(game, a_store_upgrade_costs_gold_and_raises_the_matching_stat) {
    Game game(80, 30);
    REQUIRE(game.player() != nullptr);

    for (int i = 0; i < 4; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    REQUIRE(!game.store_items().empty());

    game.open_store();
    REQUIRE(game.store_open());
    game.player()->stats().gold = 100;

    const dungeon::game::StoreItem item    = game.store_items().front();
    const int                    gold    = game.player()->stats().gold;
    const int                    attack  = game.player()->stats().attack;
    const int                    defence = game.player()->stats().defence;
    const int                    stamina = game.player()->stats().stamina;

    CHECK(game.buy_store_item(0));

    CHECK_EQ(game.player()->stats().gold, gold - item.price);
    CHECK_EQ(game.player()->stats().attack,
             attack + (item.kind == ItemKind::Sword || item.kind == ItemKind::Greatsword ? 2 : 0));
    CHECK_EQ(game.player()->stats().defence,
             defence + (item.kind == ItemKind::Shield ? 2 : 0));
    CHECK_EQ(game.player()->stats().stamina,
             stamina + (item.kind == ItemKind::Boots ? 2 : 0));
    CHECK(game.store_item_sold(0));
    CHECK(game.player()->has_item(item.kind));

    CHECK(!game.buy_store_item(0));
    REQUIRE(!game.messages().empty());
    CHECK(game.messages().back().find("already sold") != std::string::npos);
}

DUNGEON_TEST(game, store_prices_rise_with_each_store_visit) {
    Game game(80, 30);
    REQUIRE(game.player() != nullptr);

    for (int i = 0; i < 4; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    REQUIRE(!game.store_items().empty());

    std::vector<int> first_prices;
    for (const dungeon::game::StoreItem& item : game.store_items()) {
        first_prices.push_back(item.price);
    }

    // Depth ten is a boss arena, so the next store is at depth fifteen.
    for (int i = 0; i < 10; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    REQUIRE(game.store_items().size() == first_prices.size());

    for (std::size_t i = 0; i < first_prices.size(); ++i) {
        const int price  = game.store_items()[i].price;
        const int growth = first_prices[i] * dungeon::game::kStorePriceGrowthPercent / 100;

        CHECK(price > first_prices[i]);
        CHECK(price >= first_prices[i] + growth - 1);
        CHECK(price <= first_prices[i] + growth + 1);
    }
}

DUNGEON_TEST(game, buying_owned_equipment_upgrades_it_without_duplicating_it) {
    Game game(80, 30);
    REQUIRE(game.player() != nullptr);
    game.player()->stats().gold = 100;

    for (int i = 0; i < 4; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    game.open_store();
    REQUIRE(game.buy_store_item(0));
    game.close_store();

    REQUIRE(game.player()->has_item(ItemKind::Sword));
    const std::size_t inventory_size = game.player()->inventory().size();

    std::size_t swords = 0;
    std::size_t level  = 0;

    for (const dungeon::map::entities::Item& item : game.player()->inventory()) {
        if (item.kind() == ItemKind::Sword) {
            ++swords;
            level = item.level();
        }
    }

    CHECK_EQ(swords, 1U);
    CHECK_EQ(level, 1U);

    // Skip the boss at depth ten; the next store is at depth fifteen.
    for (int i = 0; i < 10; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    game.open_store();

    const int attack_before_upgrade = game.player()->stats().attack;

    REQUIRE(game.buy_store_item(0));

    CHECK_EQ(game.player()->inventory().size(), inventory_size);
    CHECK_EQ(game.player()->stats().attack, attack_before_upgrade + 2);

    swords = 0;
    level  = 0;

    for (const dungeon::map::entities::Item& item : game.player()->inventory()) {
        if (item.kind() == ItemKind::Sword) {
            ++swords;
            level = item.level();
        }
    }

    CHECK_EQ(swords, 1U);
    CHECK_EQ(level, 2U);

    const int attack_before_drop = game.player()->stats().attack;
    game.player()->drop(ItemKind::Sword);
    CHECK_EQ(game.player()->stats().attack, attack_before_drop - 4);
    CHECK(!game.player()->has_item(ItemKind::Sword));
}

DUNGEON_TEST(game, an_unaffordable_store_upgrade_is_not_sold) {
    Game game(80, 30);
    REQUIRE(game.player() != nullptr);

    for (int i = 0; i < 4; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    REQUIRE(!game.store_items().empty());

    game.open_store();
    game.player()->stats().gold = 0;

    CHECK(!game.buy_store_item(0));
    CHECK_EQ(game.player()->stats().gold, 0);
    CHECK(!game.store_item_sold(0));
    REQUIRE(!game.messages().empty());
    CHECK(game.messages().back().find("cannot afford") != std::string::npos);

    game.close_store();
    CHECK(!game.store_open());
    CHECK(!game.buy_store_item(0));
}

DUNGEON_TEST(game, direction_from_key_maps_wasd_in_both_cases) {
    CHECK(direction_from_key('w') == Direction::Up);
    CHECK(direction_from_key('W') == Direction::Up);
    CHECK(direction_from_key('s') == Direction::Down);
    CHECK(direction_from_key('S') == Direction::Down);
    CHECK(direction_from_key('a') == Direction::Left);
    CHECK(direction_from_key('A') == Direction::Left);
    CHECK(direction_from_key('d') == Direction::Right);
    CHECK(direction_from_key('D') == Direction::Right);
    CHECK(!direction_from_key('x').has_value());
}

DUNGEON_TEST(game, direction_from_key_maps_arrow_events) {
    using dungeon::controls::KeyEvent;
    using dungeon::controls::KeyKind;

    CHECK(direction_from_key(KeyEvent::from_kind(KeyKind::Up)) == Direction::Up);
    CHECK(direction_from_key(KeyEvent::from_kind(KeyKind::Down)) == Direction::Down);
    CHECK(direction_from_key(KeyEvent::from_kind(KeyKind::Left)) == Direction::Left);
    CHECK(direction_from_key(KeyEvent::from_kind(KeyKind::Right)) == Direction::Right);
    CHECK(direction_from_key(KeyEvent::from_character('w')) == Direction::Up);
    CHECK(!direction_from_key(KeyEvent::from_kind(KeyKind::Escape)).has_value());
    CHECK(!direction_from_key(KeyEvent{}).has_value());
}

DUNGEON_TEST(game, every_tenth_dungeon_replaces_the_store_with_a_boss) {
    Game game(40, 24);
    REQUIRE(game.player() != nullptr);

    for (int i = 0; i < 8; ++i) {
        game.descend();
    }

    CHECK_EQ(game.depth(), 9U);
    CHECK(!game.boss_level());
    CHECK(!game.store_level());

    game.descend();

    CHECK_EQ(game.depth(), 10U);
    CHECK(game.boss_level());
    CHECK(!game.store_level());
    CHECK(!game.map().exit_unlocked());

    const Boss* boss = game.boss();
    REQUIRE(boss != nullptr);
    CHECK_EQ(boss->tier(), 1U);
    CHECK_EQ(boss->anchors_remaining(), 0U);
    CHECK(boss->vulnerable());

    for (int i = 0; i < 5; ++i) {
        game.descend();
    }

    CHECK_EQ(game.depth(), 15U);
    CHECK(!game.boss_level());
    CHECK(game.store_level());
}

DUNGEON_TEST(game, boss_difficulty_and_anchor_count_grow_with_each_encounter) {
    Game game(40, 24);
    REQUIRE(game.player() != nullptr);

    for (int i = 0; i < 19; ++i) {
        game.descend();
    }

    REQUIRE(game.boss_level());
    const Boss* second = game.boss();
    REQUIRE(second != nullptr);
    CHECK_EQ(second->tier(), 2U);
    CHECK_EQ(second->anchors_remaining(), 2U);
    CHECK(!second->vulnerable());

    // Copy the values before descending: regeneration destroys the old boss.
    const int    second_hp     = second->max_hp();
    const int    second_attack = second->stats().attack;
    const double second_rate   = second->fireball_interval();

    for (int i = 0; i < 10; ++i) {
        game.descend();
    }

    REQUIRE(game.boss_level());
    const Boss* third = game.boss();
    REQUIRE(third != nullptr);
    CHECK_EQ(third->tier(), 3U);
    CHECK_EQ(third->anchors_remaining(), 4U);
    CHECK(third->max_hp() > second_hp);
    CHECK(third->stats().attack > second_attack);
    CHECK(third->fireball_interval() < second_rate);

    const int third_hp = third->max_hp();

    for (int i = 0; i < 20; ++i) {
        game.descend();
    }

    CHECK_EQ(game.depth(), dungeon::game::kFinalBossDepth);
    REQUIRE(game.boss_level());
    const Boss* final_boss = game.boss();
    REQUIRE(final_boss != nullptr);
    CHECK(final_boss->is_final());
    CHECK_EQ(final_boss->tier(), 5U);
    CHECK_EQ(final_boss->anchors_remaining(), 4U);
    CHECK(final_boss->max_hp() > third_hp);
    CHECK(final_boss->fireball_directions() == 8U);
}

DUNGEON_TEST(game, a_boss_takes_no_damage_until_its_anchors_are_destroyed) {
    dungeon::map::Map map;
    const MapSpec spec = map.make_boss_spec(40, 24, 2, false);
    Game game(spec);

    CHECK(game.boss_level());
    Boss* boss = game.boss();
    REQUIRE(boss != nullptr);
    REQUIRE(!boss->vulnerable());

    game.player()->place(boss->x, boss->y + 1);
    const int hp = boss->stats().hp;

    CHECK(game.move(Direction::Up));
    CHECK_EQ(boss->stats().hp, hp);
    REQUIRE(!game.messages().empty());
    CHECK(game.messages().back().find("shielded") != std::string::npos);

    destroy_all_anchors(game);

    CHECK(boss->vulnerable());
    CHECK_EQ(game.anchors_remaining(), 0U);

    game.player()->place(boss->x, boss->y + 1);
    CHECK(game.move(Direction::Up));
    CHECK(boss->stats().hp < hp);
}

DUNGEON_TEST(game, a_boss_fires_fireballs_that_travel_away_from_it) {
    dungeon::map::Map map;
    const MapSpec spec = map.make_boss_spec(40, 24, 1, false);
    Game game(spec);

    const Boss* boss = game.boss();
    REQUIRE(boss != nullptr);
    CHECK_EQ(count_fireballs(game), 0U);

    game.update(boss->fireball_interval() + 0.01);

    REQUIRE(count_fireballs(game) == 4U);

    std::vector<std::pair<std::size_t, std::size_t>> positions;
    for (const dungeon::map::EntityPtr& entity : game.map().entity_ptrs()) {
        if (dynamic_cast<const Fireball*>(entity.get()) != nullptr) {
            positions.emplace_back(entity->x, entity->y);
        }
    }

    game.update(0.19);

    std::size_t moved = 0;
    std::size_t index = 0;
    for (const dungeon::map::EntityPtr& entity : game.map().entity_ptrs()) {
        if (dynamic_cast<const Fireball*>(entity.get()) != nullptr) {
            if (index < positions.size() &&
                (entity->x != positions[index].first || entity->y != positions[index].second)) {
                ++moved;
            }
            ++index;
        }
    }

    CHECK(moved > 0U);
}

DUNGEON_TEST(game, a_fireball_hurts_the_player_and_burns_out) {
    dungeon::map::Map map;
    const MapSpec spec = map.make_boss_spec(40, 24, 1, false);
    Game game(spec);

    const Boss* boss = game.boss();
    REQUIRE(boss != nullptr);

    game.update(boss->fireball_interval() + 0.01);
    REQUIRE(count_fireballs(game) == 4U);

    const Fireball* fireball = nullptr;
    for (const dungeon::map::EntityPtr& entity : game.map().entity_ptrs()) {
        if (const auto* candidate = dynamic_cast<const Fireball*>(entity.get())) {
            fireball = candidate;
            break;
        }
    }
    REQUIRE(fireball != nullptr);

    game.player()->place(static_cast<std::size_t>(static_cast<int>(fireball->x) + fireball->dx()),
                         static_cast<std::size_t>(static_cast<int>(fireball->y) + fireball->dy()));
    const int hp = game.player()->stats().hp;

    game.update(0.19);

    CHECK(game.player()->stats().hp < hp);
    CHECK_EQ(count_fireballs(game), 3U);
}

DUNGEON_TEST(game, a_boss_giggles_on_a_timer_and_restores_its_glyph) {
    dungeon::map::Map map;
    const MapSpec spec = map.make_boss_spec(40, 24, 1, false);
    Game game(spec);

    Boss* boss = game.boss();
    REQUIRE(boss != nullptr);
    CHECK_EQ(boss->tile(), 'B');

    game.update(dungeon::map::entities::BOSS_GIGGLE_INTERVAL + 0.01);

    CHECK_EQ(boss->tile(), 'b');
    REQUIRE(!game.messages().empty());
    CHECK(game.messages().back().find("giggles") != std::string::npos);

    game.update(dungeon::map::entities::BOSS_GIGGLE_DURATION + 0.01);

    CHECK_EQ(boss->tile(), 'B');
}

DUNGEON_TEST(game, defeating_a_boss_drops_a_key_that_counts_toward_the_final_one) {
    dungeon::map::Map map;
    const MapSpec spec = map.make_boss_spec(40, 24, 1, false);
    Game game(spec);

    REQUIRE(game.boss() != nullptr);
    strike_boss_until_dead(game);

    CHECK(game.boss() == nullptr);
    CHECK_EQ(game.run_stats().bosses_defeated, 1U);
    CHECK_EQ(game.run_stats().keys_collected, 0U);
    CHECK(!game.map().exit_unlocked());

    const dungeon::map::Point boss_cell = spec.boss;
    game.player()->place(boss_cell.x, boss_cell.y - 1);
    CHECK(game.move(Direction::Down));

    CHECK(game.player()->has_key());
    CHECK_EQ(game.run_stats().keys_collected, 1U);
    CHECK(game.map().exit_unlocked());
}

DUNGEON_TEST(game, defeating_the_final_boss_grants_victory_and_fifty_keys) {
    dungeon::map::Map map;
    const MapSpec spec = map.make_boss_spec(40, 24, 5, true);
    Game game(spec);

    REQUIRE(game.boss() != nullptr);
    CHECK(!game.victory());
    destroy_all_anchors(game);

    strike_boss_until_dead(game);

    CHECK(game.boss() == nullptr);
    CHECK(game.victory());
    CHECK_EQ(game.run_stats().bosses_defeated, 1U);
    CHECK_EQ(game.run_stats().keys_collected, 1U);
    CHECK(!game.move(Direction::Up));

    game.update(1.0);
    CHECK(game.victory());

    game.restart();
    CHECK(!game.victory());
    CHECK_EQ(game.run_stats().keys_collected, 0U);
    CHECK_EQ(game.run_stats().bosses_defeated, 0U);
}
