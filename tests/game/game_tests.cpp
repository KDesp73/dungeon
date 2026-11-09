#include "test_harness.hpp"

#include "dungeon/game.hpp"
#include "dungeon/map/map.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

using dungeon::game::ActionEvent;
using dungeon::game::ActionTrigger;
using dungeon::game::Direction;
using dungeon::game::direction_from_key;
using dungeon::game::Game;
using dungeon::map::DoorSide;
using dungeon::map::MapSpec;
using dungeon::map::entities::EnemyKind;
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

    CHECK_EQ(game.player()->stats().hp, hp_before - 1);
    CHECK(!game.game_over());
    REQUIRE(!game.messages().empty());
    CHECK_EQ(game.messages().back(), std::string("The Goblin hits you for 1."));
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
}

DUNGEON_TEST(game, a_brute_drops_more_coins_than_a_rat) {
    MapSpec spec = make_test_spec();
    spec.enemies = {{EnemyKind::Brute, 4, 3}};

    Game game(spec);
    REQUIRE(game.player() != nullptr);

    for (int hit = 0; hit < 5; ++hit) {
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
