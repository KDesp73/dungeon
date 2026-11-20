#include "test_harness.hpp"

#include "agent/agent.hpp"
#include "agent/kev_agent.hpp"
#include "agent/kev_client.hpp"
#include "agent/observation.hpp"
#include "agent/policy.hpp"
#include "agent/scripted_agent.hpp"
#include "dungeon/controls.hpp"
#include "dungeon/game.hpp"
#include "dungeon/map/map.hpp"
#include "dungeon/scene/game_scene.hpp"
#include "dungeon/scene/scene_manager.hpp"
#include "dungeon/scene/store_scene.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

using dungeon::agent::AgentDecision;
using dungeon::agent::AgentObservation;
using dungeon::agent::KevAgent;
using dungeon::agent::KevAnswers;
using dungeon::agent::KevTransport;
using dungeon::agent::SceneKind;
using dungeon::controls::KeyEvent;
using dungeon::controls::KeyKind;
using dungeon::game::Game;
using dungeon::map::DoorSide;
using dungeon::map::MapSpec;
using dungeon::scene::GameScene;
using dungeon::scene::GameSceneConfig;
using dungeon::scene::SceneManager;

const char* kKevAnswer = R"json({
    "model": "kev-latest",
    "answers": {
        "goal": {"type": "choice", "choice": "find_key", "confidence": 0.4},
        "engage": {"type": "noul", "noul": 0.6},
        "danger": {"type": "score", "score": 1.1},
        "move": {"type": "choice", "choice": "left", "confidence": 0.3}
    }
})json";

MapSpec make_spec() {
    MapSpec spec;
    spec.width       = 12;
    spec.height      = 7;
    spec.rooms       = {{1, 1, 5, 5}, {7, 1, 4, 5}};
    spec.connections = {{0, 1, DoorSide::Right, DoorSide::Left}};
    spec.entrance    = {2, 2};
    spec.exit        = {9, 2};
    return spec;
}

AgentObservation make_observation() {
    AgentObservation observation;
    observation.scene         = SceneKind::Game;
    observation.tiles         = {"#####", "#...#", "#####"};
    observation.visible       = observation.tiles;
    observation.player        = {2, 1};
    observation.exit_cell     = {3, 1};
    observation.key_on_map    = true;
    observation.key_cell      = {1, 1};
    observation.hp            = 90;
    observation.max_hp        = 100;
    observation.gold          = 20;
    observation.attack        = 10;
    observation.defence       = 5;
    return observation;
}

#ifdef DUNGEON_ENABLE_KEV

template <typename Predicate>
bool wait_for(Predicate predicate, std::chrono::milliseconds timeout = 3000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::yield();
    }
    return predicate();
}

class FakeTransport final : public KevTransport {
  public:
    std::function<void(std::size_t)> on_post;

    [[nodiscard]] Reply post(const std::string& body) override {
        std::size_t call = 0;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            call = ++_calls;
            _bodies.push_back(body);
        }
        if (on_post) {
            on_post(call);
        }
        return Reply{true, 200, kKevAnswer, ""};
    }

    [[nodiscard]] std::size_t calls() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _calls;
    }

    [[nodiscard]] std::vector<std::string> bodies() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _bodies;
    }

  private:
    mutable std::mutex       _mutex;
    std::size_t              _calls  = 0;
    std::vector<std::string> _bodies;
};

#endif // DUNGEON_ENABLE_KEV

} // namespace

DUNGEON_TEST(agent, observation_json_is_stable_for_a_fixed_spec) {
    const MapSpec spec = make_spec();

    SceneManager first;
    SceneManager second;
    GameSceneConfig config;
    config.layout = spec;
    first.push(std::make_unique<GameScene>(config));
    second.push(std::make_unique<GameScene>(config));

    const auto* first_game  = first.find<GameScene>();
    const auto* second_game = second.find<GameScene>();
    REQUIRE(first_game != nullptr);
    REQUIRE(second_game != nullptr);

    const std::string one =
        dungeon::agent::AgentObservation::from(first_game->game(), first).to_json();
    const std::string two =
        dungeon::agent::AgentObservation::from(second_game->game(), second).to_json();
    CHECK_EQ(one, two);
    CHECK(one.find("\"game\":\"dungeon\"") != std::string::npos);
    CHECK(one.find("\"depth\":1") != std::string::npos);
    CHECK(one.find("\"visible_map\"") != std::string::npos);
}

DUNGEON_TEST(agent, fake_kev_answers_produce_expected_keys) {
    const AgentObservation observation = make_observation();

    KevAnswers answers;
    answers.received  = true;
    answers.move.choice = "right";
    answers.goal.choice = "find_key";
    answers.danger.score = 0.5;

    const AgentDecision decision = dungeon::agent::decide(observation, answers);
    REQUIRE(decision.keys.size() == 1);
    CHECK_EQ(decision.keys.front(), KeyEvent::from_kind(KeyKind::Right));
}

DUNGEON_TEST(agent, illegal_options_are_never_emitted) {
    const AgentObservation observation = make_observation();

    KevAnswers answers;
    answers.received    = true;
    answers.move.choice = "teleport";
    answers.goal.choice = "find_key";

    const AgentDecision decision = dungeon::agent::decide(observation, answers);
    REQUIRE(decision.keys.size() == 1);
    CHECK_EQ(decision.keys.front(), KeyEvent::from_kind(KeyKind::Left));

    const auto legal = dungeon::agent::legal_actions(observation);
    bool emitted_is_legal = false;
    for (const auto& action : legal) {
        emitted_is_legal = emitted_is_legal || action.key == decision.keys.front();
    }
    CHECK(emitted_is_legal);
}

DUNGEON_TEST(agent, store_purchase_selects_requested_item) {
    AgentObservation observation = make_observation();
    observation.scene = SceneKind::Store;
    observation.store_selection = 0;
    observation.store_items = {
        {"Sword", "+2 attack", 10, false, true},
        {"Shield", "+2 defence", 12, false, true},
        {"Boots", "+2 stamina", 8, false, true},
    };

    KevAnswers answers;
    answers.received  = true;
    answers.buy.choice = "2";

    const AgentDecision decision = dungeon::agent::decide(observation, answers);
    // With three items, index 2 is one step backwards via the store's wrap-around.
    REQUIRE(decision.keys.size() == 2);
    CHECK_EQ(decision.keys[0], KeyEvent::from_kind(KeyKind::Up));
    CHECK_EQ(decision.keys[1], KeyEvent::from_character('\n'));
}

DUNGEON_TEST(agent, request_offers_only_legal_answers) {
    const AgentObservation observation = make_observation();
    const std::string request = dungeon::agent::build_kev_request(observation, 7);
    CHECK(request.find("\"observation_id\":7") != std::string::npos);
    CHECK(request.find("\"goal\"") != std::string::npos);
    CHECK(request.find("\"engage\"") != std::string::npos);
    CHECK(request.find("\"danger\"") != std::string::npos);
    CHECK(request.find("\"move\"") != std::string::npos);
    CHECK(request.find("\"up\"") != std::string::npos);
    CHECK(request.find("\"teleport\"") == std::string::npos);
}

DUNGEON_TEST(agent, malformed_kev_response_falls_back) {
    KevAnswers answers;
    CHECK(!dungeon::agent::parse_kev_response("not json", answers));
    CHECK(!answers.received);

    CHECK(dungeon::agent::parse_kev_response(kKevAnswer, answers));
    CHECK(answers.received);
    CHECK_EQ(answers.move.choice, std::string("left"));
    CHECK(answers.danger.score > 1.0);
}

#ifdef DUNGEON_ENABLE_KEV

DUNGEON_TEST(agent, stale_observation_replies_are_discarded) {
    SceneManager scenes;
    GameSceneConfig config;
    config.layout = make_spec();
    scenes.push(std::make_unique<GameScene>(config));
    const auto* game_scene = scenes.find<GameScene>();
    REQUIRE(game_scene != nullptr);

    auto transport = std::make_unique<FakeTransport>();
    FakeTransport* fake = transport.get();
    std::promise<void> first_gate;
    std::promise<void> second_gate;
    const std::shared_future<void> first = first_gate.get_future().share();
    const std::shared_future<void> second = second_gate.get_future().share();
    fake->on_post = [first, second](std::size_t call) {
        if (call == 1) {
            first.wait();
        } else if (call == 2) {
            second.wait();
        }
    };

    KevAgent agent("http://127.0.0.1:1", std::move(transport), 1'000'000);
    agent.observe(game_scene->game(), scenes);
    agent.observe(game_scene->game(), scenes);

    // The worker blocks inside the first POST, so the second observation
    // supersedes it before a reply is stored.
    REQUIRE(wait_for([&]() { return fake->calls() >= 1; }));
    first_gate.set_value();
    // Once the second POST starts, the stale reply for id 1 is already stored.
    REQUIRE(wait_for([&]() { return fake->calls() >= 2; }));

    CHECK(!agent.poll().has_value());
    CHECK_EQ(agent.stale_replies(), std::size_t(1));

    second_gate.set_value();
    REQUIRE(wait_for([&]() { return agent.poll().has_value(); }));
    CHECK_EQ(agent.current_id(), std::size_t(2));
    CHECK(agent.stale_replies() >= 1);
    CHECK(fake->bodies().size() >= 2);
}

#endif // DUNGEON_ENABLE_KEV

DUNGEON_TEST(agent, scripted_agent_makes_progress_without_a_server) {
    SceneManager scenes;
    GameSceneConfig config;
    config.layout = make_spec();
    scenes.push(std::make_unique<GameScene>(config));
    const auto* game_scene = scenes.find<GameScene>();
    REQUIRE(game_scene != nullptr);

    dungeon::agent::ScriptedAgent bot;
    bot.observe(game_scene->game(), scenes);
    const auto decision = bot.poll();
    REQUIRE(decision.has_value());
    CHECK(!decision->keys.empty());
    CHECK(!bot.poll().has_value());
}
