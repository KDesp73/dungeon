#include "test_harness.hpp"

#include "dungeon/controls.hpp"
#include "dungeon/game.hpp"
#include "dungeon/map/map.hpp"
#include "dungeon/scene/game_scene.hpp"
#include "dungeon/scene/pause_scene.hpp"
#include "dungeon/scene/scene_manager.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <utility>

using dungeon::controls::KeyEvent;
using dungeon::controls::KeyKind;
using dungeon::map::MapSpec;
using dungeon::map::entities::EnemyKind;
using dungeon::scene::GameScene;
using dungeon::scene::GameSceneConfig;
using dungeon::scene::PauseScene;
using dungeon::scene::SceneContext;
using dungeon::scene::SceneManager;

namespace {

MapSpec make_test_spec() {
    MapSpec spec;
    spec.width       = 20;
    spec.height      = 10;
    spec.rooms       = {{2, 2, 12, 6}};
    spec.connections = {};
    spec.entrance    = {3, 3};
    spec.exit        = {6, 3};
    spec.items       = {{dungeon::map::entities::ItemKind::Coin, 4, 3}};
    return spec;
}

} // namespace

DUNGEON_TEST(scene, game_scene_builds_the_configured_layout) {
    GameSceneConfig config;
    config.layout = make_test_spec();

    GameScene scene(std::move(config));

    CHECK_EQ(scene.game().map().width(), 20U);
    CHECK_EQ(scene.game().map().height(), 10U);
    REQUIRE(scene.game().player() != nullptr);
    CHECK_EQ(scene.game().player()->x, 3U);
    CHECK_EQ(scene.game().player()->y, 3U);
}

DUNGEON_TEST(scene, game_scene_moves_the_player_and_collects_items) {
    SceneManager manager;
    SceneContext context{manager};

    GameSceneConfig config;
    config.layout = make_test_spec();

    manager.push(std::make_unique<GameScene>(std::move(config)));

    auto* scene = dynamic_cast<GameScene*>(manager.top());
    REQUIRE(scene != nullptr);

    CHECK(manager.handle_key(KeyEvent::from_character('d'), context));
    REQUIRE(scene->game().player() != nullptr);
    CHECK_EQ(scene->game().player()->x, 4U);
    CHECK_EQ(scene->game().player()->stats().gold, 1);
}

DUNGEON_TEST(scene, game_scene_uses_a_health_potion_with_h) {
    SceneManager manager;
    SceneContext context{manager};

    MapSpec spec = make_test_spec();
    spec.items   = {{dungeon::map::entities::ItemKind::HealthPotion, 4, 3}};

    GameSceneConfig config;
    config.layout = std::move(spec);

    manager.push(std::make_unique<GameScene>(std::move(config)));

    auto* scene = dynamic_cast<GameScene*>(manager.top());
    REQUIRE(scene != nullptr);
    REQUIRE(scene->game().player() != nullptr);

    REQUIRE(manager.handle_key(KeyEvent::from_character('d'), context));
    REQUIRE(scene->game().player()->has_item(dungeon::map::entities::ItemKind::HealthPotion));

    scene->game().player()->stats().hp = 50;

    CHECK(manager.handle_key(KeyEvent::from_character('h'), context));

    const int hp = scene->game().player()->stats().hp;
    CHECK(hp >= 50 + dungeon::map::entities::HEALTH_POTION_HEAL_MIN);
    CHECK(hp <= 50 + dungeon::map::entities::HEALTH_POTION_HEAL_MAX);
    CHECK(!scene->game().player()->has_item(dungeon::map::entities::ItemKind::HealthPotion));
}

DUNGEON_TEST(scene, game_scene_escape_opens_the_pause_menu) {
    SceneManager manager;
    SceneContext context{manager};

    GameSceneConfig config;
    config.layout       = make_test_spec();
    config.save_handler = [](const dungeon::game::Game& /*game*/) { return true; };

    manager.push(std::make_unique<GameScene>(std::move(config)));

    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Escape), context));
    CHECK_EQ(manager.size(), 2U);
    CHECK(dynamic_cast<PauseScene*>(manager.top()) != nullptr);
}

DUNGEON_TEST(scene, game_scene_pause_menu_saves_and_quits) {
    SceneManager manager;
    SceneContext context{manager};

    bool saved = false;

    GameSceneConfig config;
    config.layout       = make_test_spec();
    config.save_handler = [&saved](const dungeon::game::Game& /*game*/) {
        saved = true;
        return true;
    };

    manager.push(std::make_unique<GameScene>(std::move(config)));
    REQUIRE(manager.handle_key(KeyEvent::from_kind(KeyKind::Escape), context));

    auto* pause = dynamic_cast<PauseScene*>(manager.top());
    REQUIRE(pause != nullptr);

    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Down), context));
    CHECK(manager.handle_key(KeyEvent::from_character('\n'), context));
    CHECK(saved);
    CHECK_EQ(pause->status(), std::string("Game saved."));

    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Down), context));
    CHECK(manager.handle_key(KeyEvent::from_character('\n'), context));
    CHECK(context.quit_requested);
}

DUNGEON_TEST(scene, game_scene_updates_the_enemy_simulation) {
    SceneManager manager;
    SceneContext context{manager};

    MapSpec spec = make_test_spec();
    spec.enemies = {{EnemyKind::Normal, 4, 3}};

    GameSceneConfig config;
    config.layout = std::move(spec);

    manager.push(std::make_unique<GameScene>(std::move(config)));

    auto* scene = dynamic_cast<GameScene*>(manager.top());
    REQUIRE(scene != nullptr);
    REQUIRE(scene->game().player() != nullptr);
    const int hp_before = scene->game().player()->stats().hp;

    manager.update(std::chrono::milliseconds(2000), context);

    CHECK_EQ(scene->game().player()->stats().hp, hp_before - 1);
}

DUNGEON_TEST(scene, game_scene_restarts_after_the_player_dies) {
    SceneManager manager;
    SceneContext context{manager};

    MapSpec spec;
    spec.width    = 40;
    spec.height   = 24;
    spec.rooms    = {{1, 1, 38, 22}};
    spec.entrance = {3, 3};
    spec.exit     = {30, 12};
    spec.enemies  = {{EnemyKind::Brute, 4, 3}};

    GameSceneConfig config;
    config.layout = std::move(spec);

    manager.push(std::make_unique<GameScene>(std::move(config)));

    auto* scene = dynamic_cast<GameScene*>(manager.top());
    REQUIRE(scene != nullptr);
    REQUIRE(scene->game().player() != nullptr);
    scene->game().player()->stats().hp = 1;

    manager.update(std::chrono::milliseconds(2000), context);
    REQUIRE(scene->game().game_over());

    CHECK(manager.handle_key(KeyEvent::from_character('r'), context));
    CHECK(!scene->game().game_over());
    REQUIRE(scene->game().player() != nullptr);
    CHECK(scene->game().player()->alive());
}

DUNGEON_TEST(scene, game_scene_pause_menu_reports_when_saving_is_unavailable) {
    SceneManager manager;
    SceneContext context{manager};

    GameSceneConfig config;
    config.layout = make_test_spec();

    manager.push(std::make_unique<GameScene>(std::move(config)));
    REQUIRE(manager.handle_key(KeyEvent::from_kind(KeyKind::Escape), context));

    auto* pause = dynamic_cast<PauseScene*>(manager.top());
    REQUIRE(pause != nullptr);

    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Down), context));
    CHECK(manager.handle_key(KeyEvent::from_character('\n'), context));
    CHECK_EQ(pause->status(), std::string("Saving is not available yet."));
}
