#include "test_harness.hpp"

#include "dungeon/game.hpp"
#include "dungeon/map/map.hpp"

#include <cstddef>
#include <vector>

using dungeon::game::ActionEvent;
using dungeon::game::ActionTrigger;
using dungeon::game::Direction;
using dungeon::game::direction_from_key;
using dungeon::game::Game;
using dungeon::map::DoorSide;
using dungeon::map::MapSpec;
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

DUNGEON_TEST(game, stepping_onto_an_interactable_triggers_only_a_step_action) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Chest, 4, 3}};

    Game                        game(spec);
    std::vector<RecordedAction> actions;
    record_actions(game, actions);

    CHECK(game.move(Direction::Right));

    CHECK(has_action(actions, ActionTrigger::Step, '?', 4, 3));
    CHECK_EQ(count_actions(actions, ActionTrigger::Nearby), 0U);
}

DUNGEON_TEST(game, a_hook_can_remove_the_item_it_receives) {
    MapSpec spec = make_test_spec();
    spec.items   = {{ItemKind::Coin, 4, 3}};

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
