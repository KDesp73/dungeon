#include "test_harness.hpp"

#include "dungeon/game.hpp"
#include "dungeon/map/map.hpp"
#include "dungeon/save/save.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

using dungeon::game::ActionEvent;
using dungeon::game::Direction;
using dungeon::game::Game;
using dungeon::map::DoorSide;
using dungeon::map::MapSpec;
using dungeon::map::entities::Boss;
using dungeon::map::entities::Chest;
using dungeon::map::entities::EnemyKind;
using dungeon::map::entities::ItemKind;
using dungeon::save::SaveResult;

namespace {

MapSpec make_test_spec() {
    MapSpec spec;
    spec.width       = 40;
    spec.height      = 20;
    spec.rooms       = {{2, 2, 16, 12}, {22, 2, 16, 12}};
    spec.connections = {{0, 1, DoorSide::Right, DoorSide::Left}};
    spec.entrance    = {3, 3};
    spec.exit        = {35, 3};
    spec.items       = {{ItemKind::Coin, 4, 3},
                        {ItemKind::HealthPotion, 5, 3},
                        {ItemKind::Chest, 6, 3, {ItemKind::Coin, ItemKind::Key}}};
    spec.enemies     = {{EnemyKind::Fast, 8, 3}, {EnemyKind::Brute, 9, 3}};
    spec.has_boss    = true;
    spec.boss        = {28, 8};
    spec.boss_tier   = 2;
    spec.anchors     = {{25, 5}, {31, 11}};
    return spec;
}

std::string temp_save_path(const char* suffix) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto name =
        "dungeon-save-test-" + std::to_string(stamp) + "-" + std::string(suffix) + ".sav";
    return (std::filesystem::temp_directory_path() / name).string();
}

std::vector<std::uint8_t> read_bytes(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        return {};
    }

    const auto size = input.tellg();
    if (size < 0) {
        return {};
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    return bytes;
}

void write_bytes(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
}

const Chest* find_chest(const Game& game) {
    for (const dungeon::map::EntityPtr& entity : game.map().entity_ptrs()) {
        if (const auto* chest = dynamic_cast<const Chest*>(entity.get()); chest != nullptr) {
            return chest;
        }
    }
    return nullptr;
}

void check_player_matches(const Game& lhs, const Game& rhs) {
    REQUIRE(lhs.player() != nullptr);
    REQUIRE(rhs.player() != nullptr);

    const auto& a = lhs.player()->stats();
    const auto& b = rhs.player()->stats();
    CHECK_EQ(b.level, a.level);
    CHECK_EQ(b.hp, a.hp);
    CHECK_EQ(b.attack, a.attack);
    CHECK_EQ(b.defence, a.defence);
    CHECK_EQ(b.stamina, a.stamina);
    CHECK_EQ(b.gold, a.gold);
}

} // namespace

DUNGEON_TEST(save, round_trip_preserves_the_complete_run) {
    const std::string path = temp_save_path("roundtrip");

    Game source(make_test_spec());
    REQUIRE(source.player() != nullptr);
    source.player()->stats().hp      = 37;
    source.player()->stats().gold    = 123;
    source.player()->stats().attack  = 21;
    source.player()->stats().defence = 9;
    source.update(0.35);

    const SaveResult saved = dungeon::save::save_to_file(source, path);
    REQUIRE(saved.ok);
    CHECK(dungeon::save::save_file_exists(path));

    Game loaded;
    const SaveResult result = dungeon::save::load_from_file(loaded, path);
    REQUIRE(result.ok);

    CHECK_EQ(loaded.game_over(), source.game_over());
    CHECK_EQ(loaded.victory(), source.victory());
    CHECK_EQ(loaded.boss_level(), source.boss_level());
    CHECK_EQ(loaded.store_level(), source.store_level());
    CHECK_EQ(loaded.store_open(), source.store_open());
    CHECK_EQ(loaded.depth(), source.depth());
    CHECK(loaded.messages() == source.messages());

    CHECK_EQ(loaded.run_stats().dungeons_completed, source.run_stats().dungeons_completed);
    CHECK_EQ(loaded.run_stats().enemies_killed, source.run_stats().enemies_killed);
    CHECK_EQ(loaded.run_stats().potions_used, source.run_stats().potions_used);
    CHECK_EQ(loaded.run_stats().gold_collected, source.run_stats().gold_collected);
    CHECK_EQ(loaded.run_stats().keys_collected, source.run_stats().keys_collected);
    CHECK_EQ(loaded.run_stats().bosses_defeated, source.run_stats().bosses_defeated);

    check_player_matches(source, loaded);

    CHECK_EQ(loaded.map().width(), source.map().width());
    CHECK_EQ(loaded.map().height(), source.map().height());
    CHECK_EQ(loaded.map().seed(), source.map().seed());
    CHECK_EQ(loaded.map().entrance().x, source.map().entrance().x);
    CHECK_EQ(loaded.map().entrance().y, source.map().entrance().y);
    CHECK_EQ(loaded.map().exit().x, source.map().exit().x);
    CHECK_EQ(loaded.map().exit().y, source.map().exit().y);
    CHECK_EQ(loaded.map().exit_unlocked(), source.map().exit_unlocked());

    for (std::size_t y = 0; y < source.map().height(); ++y) {
        for (std::size_t x = 0; x < source.map().width(); ++x) {
            CHECK(loaded.map().at(x, y) == source.map().at(x, y));
        }
    }

    const auto& original_entities = source.map().entity_ptrs();
    const auto& restored_entities = loaded.map().entity_ptrs();
    REQUIRE(restored_entities.size() == original_entities.size());
    for (std::size_t i = 0; i < original_entities.size(); ++i) {
        CHECK_EQ(restored_entities[i]->id(), original_entities[i]->id());
        CHECK_EQ(restored_entities[i]->x, original_entities[i]->x);
        CHECK_EQ(restored_entities[i]->y, original_entities[i]->y);
        CHECK_EQ(restored_entities[i]->tile(), original_entities[i]->tile());
        const auto& restored_entity = *restored_entities[i];
        const auto& original_entity = *original_entities[i];
        CHECK(typeid(restored_entity) == typeid(original_entity));
    }

    const Boss* original_boss = source.boss();
    const Boss* restored_boss = loaded.boss();
    REQUIRE(original_boss != nullptr);
    REQUIRE(restored_boss != nullptr);
    CHECK_EQ(restored_boss->tier(), original_boss->tier());
    CHECK_EQ(restored_boss->anchors_remaining(), original_boss->anchors_remaining());
    CHECK_EQ(restored_boss->volleys_fired(), original_boss->volleys_fired());
    CHECK_EQ(restored_boss->volley_rotation(), original_boss->volley_rotation());
    CHECK(restored_boss->attack_pattern() == original_boss->attack_pattern());

    const Chest* original_chest = find_chest(source);
    const Chest* restored_chest = find_chest(loaded);
    REQUIRE(original_chest != nullptr);
    REQUIRE(restored_chest != nullptr);
    REQUIRE(restored_chest->contents().size() == original_chest->contents().size());
    for (std::size_t i = 0; i < original_chest->contents().size(); ++i) {
        CHECK(restored_chest->contents()[i].kind() == original_chest->contents()[i].kind());
        CHECK_EQ(restored_chest->contents()[i].level(), original_chest->contents()[i].level());
    }

    // Both generators must continue from the exact same RNG stream.
    source.descend();
    loaded.descend();
    CHECK_EQ(loaded.depth(), source.depth());
    CHECK_EQ(loaded.map().seed(), source.map().seed());
    CHECK_EQ(loaded.map().entity_ptrs().size(), source.map().entity_ptrs().size());
    CHECK(loaded.messages() == source.messages());

    std::filesystem::remove(path);
}

DUNGEON_TEST(save, loading_preserves_an_existing_action_hook) {
    const std::string path = temp_save_path("hook");
    Game source(make_test_spec());
    REQUIRE(dungeon::save::save_to_file(source, path).ok);

    Game loaded;
    int  hook_calls = 0;
    loaded.set_action_hook([&hook_calls](const ActionEvent&) { ++hook_calls; });
    REQUIRE(dungeon::save::load_from_file(loaded, path).ok);

    REQUIRE(loaded.player() != nullptr);
    REQUIRE(loaded.move(Direction::Right));
    CHECK_EQ(hook_calls, 1);

    std::filesystem::remove(path);
}

DUNGEON_TEST(save, corrupt_files_are_rejected_without_touching_the_game) {
    const std::string path = temp_save_path("corrupt");
    Game source(make_test_spec());
    REQUIRE(dungeon::save::save_to_file(source, path).ok);

    auto bytes = read_bytes(path);
    REQUIRE(bytes.size() > 40U);
    bytes[50] ^= 0xFFU;
    write_bytes(path, bytes);

    Game game(make_test_spec());
    const std::size_t depth_before = game.depth();
    const std::size_t entities_before = game.map().entity_ptrs().size();

    const SaveResult result = dungeon::save::load_from_file(game, path);
    CHECK(!result.ok);
    CHECK_EQ(game.depth(), depth_before);
    CHECK_EQ(game.map().entity_ptrs().size(), entities_before);

    bytes[0] = static_cast<std::uint8_t>('X');
    write_bytes(path, bytes);
    CHECK(!dungeon::save::load_from_file(game, path).ok);

    std::filesystem::remove(path);
}
