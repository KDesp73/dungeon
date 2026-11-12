#include "test_harness.hpp"

#include "dungeon/controls.hpp"
#include "dungeon/game.hpp"
#include "dungeon/map/map.hpp"
#include "dungeon/scene/game_scene.hpp"
#include "dungeon/scene/pause_scene.hpp"
#include "dungeon/scene/scene_manager.hpp"
#include "dungeon/scene/store_scene.hpp"

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
using dungeon::scene::StoreScene;

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

MapSpec make_large_spec() {
    MapSpec spec;
    spec.width    = 80;
    spec.height   = 30;
    spec.rooms    = {{1, 1, 78, 28}};
    spec.entrance = {3, 3};
    spec.exit     = {6, 3};
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

    CHECK_EQ(scene->game().player()->stats().hp, hp_before - 5);
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

DUNGEON_TEST(scene, game_scene_opens_the_shop_menu_near_the_shopkeeper) {
    SceneManager manager;
    SceneContext context{manager};

    GameSceneConfig config;
    config.layout = make_large_spec();

    manager.push(std::make_unique<GameScene>(std::move(config)));

    auto* scene = dynamic_cast<GameScene*>(manager.top());
    REQUIRE(scene != nullptr);

    for (int i = 0; i < 4; ++i) {
        scene->game().descend();
    }

    REQUIRE(scene->game().store_level());
    REQUIRE(scene->game().player() != nullptr);
    CHECK(manager.size() == 1U);

    const dungeon::map::entities::Shopkeeper* shopkeeper = nullptr;
    for (const dungeon::map::EntityPtr& entity : scene->game().map().entity_ptrs()) {
        if (const auto* candidate =
                dynamic_cast<const dungeon::map::entities::Shopkeeper*>(entity.get())) {
            shopkeeper = candidate;
        }
    }

    REQUIRE(shopkeeper != nullptr);

    scene->game().player()->place(shopkeeper->x,
                                  shopkeeper->y + dungeon::game::kShopkeeperReach);
    manager.update(std::chrono::milliseconds(0), context);

    CHECK_EQ(manager.size(), 2U);
    auto* store = dynamic_cast<StoreScene*>(manager.top());
    REQUIRE(store != nullptr);

    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Escape), context));
    CHECK_EQ(manager.size(), 1U);
    CHECK(!scene->game().store_open());

    // Still in range: the menu stays closed until the player walks away and
    // comes back.
    manager.update(std::chrono::milliseconds(0), context);
    CHECK_EQ(manager.size(), 1U);

    const dungeon::map::Point entrance = scene->game().map().entrance();
    scene->game().player()->place(entrance.x, entrance.y);
    manager.update(std::chrono::milliseconds(0), context);
    CHECK_EQ(manager.size(), 1U);

    scene->game().player()->place(shopkeeper->x,
                                  shopkeeper->y + dungeon::game::kShopkeeperReach);
    manager.update(std::chrono::milliseconds(0), context);
    CHECK_EQ(manager.size(), 2U);
}

DUNGEON_TEST(scene, store_scene_draws_its_catalog) {
    dungeon::game::Game game(80, 30);
    REQUIRE(game.player() != nullptr);

    for (int i = 0; i < 4; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    game.open_store();

    SceneManager manager;
    SceneContext context{manager};

    manager.push(std::make_unique<StoreScene>(game));

    dungeon::display::FrameBuffer buffer(80, 24);
    manager.draw(buffer, context);

    const std::string frame = buffer.render();
    CHECK(frame.find("STORE") != std::string::npos);
    CHECK(frame.find("Sword") != std::string::npos);
    CHECK(frame.find("Gold:") != std::string::npos);
}

DUNGEON_TEST(scene, store_scene_buys_an_upgrade_and_leaves) {
    dungeon::game::Game game(80, 30);
    REQUIRE(game.player() != nullptr);

    for (int i = 0; i < 4; ++i) {
        game.descend();
    }

    REQUIRE(game.store_level());
    game.open_store();
    game.player()->stats().gold = 100;

    SceneManager manager;
    SceneContext context{manager};

    manager.push(std::make_unique<StoreScene>(game));

    auto* store = dynamic_cast<StoreScene*>(manager.top());
    REQUIRE(store != nullptr);

    const int attack_before = game.player()->stats().attack;

    CHECK(manager.handle_key(KeyEvent::from_character('\n'), context));
    CHECK_EQ(store->status(), std::string("Bought the Sword for 10 gold."));
    CHECK_EQ(game.player()->stats().attack, attack_before + 2);

    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Escape), context));
    CHECK(manager.empty());
    CHECK(!game.store_open());
}
