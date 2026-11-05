#include "test_harness.hpp"

#include "dungeon/controls.hpp"
#include "dungeon/display/buffer.hpp"
#include "dungeon/scene/pause_scene.hpp"
#include "dungeon/scene/scene_manager.hpp"
#include "dungeon/scene/title_scene.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

using dungeon::controls::KeyEvent;
using dungeon::controls::KeyKind;
using dungeon::display::FrameBuffer;
using dungeon::scene::PauseOption;
using dungeon::scene::PauseScene;
using dungeon::scene::PauseSceneConfig;
using dungeon::scene::Scene;
using dungeon::scene::SceneContext;
using dungeon::scene::SceneManager;
using dungeon::scene::TitleScene;
using dungeon::scene::TitleSceneConfig;

namespace {

struct SceneStats {
    int enters  = 0;
    int exits   = 0;
    int updates = 0;
    int draws   = 0;
    int keys    = 0;
};

class StubScene final : public Scene {
  public:
    explicit StubScene(SceneStats& stats, bool update_below = false, bool draw_below = false)
        : _stats(stats), _update_below(update_below), _draw_below(draw_below) {}

    void on_enter() override { ++_stats.enters; }
    void on_exit() override { ++_stats.exits; }
    void update(dungeon::scene::Duration /*delta*/, SceneContext& /*context*/) override {
        ++_stats.updates;
    }
    bool handle_key(const KeyEvent& /*key*/, SceneContext& /*context*/) override {
        ++_stats.keys;
        return true;
    }
    void draw(FrameBuffer& /*buffer*/, SceneContext& /*context*/) override { ++_stats.draws; }

    [[nodiscard]] bool updates_underlying() const noexcept override { return _update_below; }
    [[nodiscard]] bool draws_underlying() const noexcept override { return _draw_below; }

  private:
    SceneStats& _stats;
    bool        _update_below;
    bool        _draw_below;
};

class ActionScene final : public Scene {
  public:
    explicit ActionScene(std::function<void(SceneContext&)> action) : _action(std::move(action)) {}

    bool handle_key(const KeyEvent& /*key*/, SceneContext& context) override {
        _action(context);
        return true;
    }

    void draw(FrameBuffer& /*buffer*/, SceneContext& /*context*/) override {}

  private:
    std::function<void(SceneContext&)> _action;
};

std::string row_text(const FrameBuffer& buffer, std::size_t y) {
    std::string text;
    text.reserve(buffer.width());
    for (std::size_t x = 0; x < buffer.width(); ++x) {
        text.push_back(buffer.at(x, y));
    }
    return text;
}

bool buffer_contains(const FrameBuffer& buffer, std::string_view text) {
    if (text.empty()) {
        return true;
    }

    for (std::size_t y = 0; y < buffer.height(); ++y) {
        if (row_text(buffer, y).find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

DUNGEON_TEST(scene, manager_calls_enter_and_exit_for_the_stack) {
    SceneManager manager;

    SceneStats base;
    SceneStats overlay;

    manager.push(std::make_unique<StubScene>(base));
    CHECK_EQ(manager.size(), 1U);
    CHECK_EQ(base.enters, 1);

    manager.push(std::make_unique<StubScene>(overlay));
    CHECK_EQ(manager.size(), 2U);
    CHECK_EQ(overlay.enters, 1);

    manager.pop();
    CHECK_EQ(manager.size(), 1U);
    CHECK_EQ(overlay.exits, 1);

    manager.clear();
    CHECK(manager.empty());
    CHECK_EQ(base.exits, 1);
}

DUNGEON_TEST(scene, manager_replace_swaps_the_top_scene) {
    SceneManager manager;

    SceneStats first;
    SceneStats second;

    manager.push(std::make_unique<StubScene>(first));
    manager.replace(std::make_unique<StubScene>(second));

    CHECK_EQ(manager.size(), 1U);
    CHECK_EQ(first.exits, 1);
    CHECK_EQ(second.enters, 1);
}

DUNGEON_TEST(scene, only_the_top_scene_receives_input) {
    SceneManager manager;
    SceneContext context{manager};

    SceneStats base;
    SceneStats overlay;

    manager.push(std::make_unique<StubScene>(base));
    manager.push(std::make_unique<StubScene>(overlay));

    CHECK(manager.handle_key(KeyEvent::from_character('x'), context));
    CHECK_EQ(base.keys, 0);
    CHECK_EQ(overlay.keys, 1);
}

DUNGEON_TEST(scene, an_opaque_scene_freezes_scenes_below_it) {
    SceneManager manager;
    SceneContext context{manager};

    SceneStats base;
    SceneStats overlay;

    manager.push(std::make_unique<StubScene>(base));
    manager.push(std::make_unique<StubScene>(overlay));

    manager.update(dungeon::scene::Duration{0}, context);

    CHECK_EQ(base.updates, 0);
    CHECK_EQ(overlay.updates, 1);
}

DUNGEON_TEST(scene, a_transparent_scene_updates_scenes_below_it) {
    SceneManager manager;
    SceneContext context{manager};

    SceneStats base;
    SceneStats overlay;

    manager.push(std::make_unique<StubScene>(base));
    manager.push(std::make_unique<StubScene>(overlay, true, true));

    manager.update(dungeon::scene::Duration{0}, context);

    CHECK_EQ(base.updates, 1);
    CHECK_EQ(overlay.updates, 1);
}

DUNGEON_TEST(scene, an_overlay_draws_scenes_below_it) {
    SceneManager manager;
    SceneContext context{manager};

    SceneStats base;
    SceneStats overlay;

    manager.push(std::make_unique<StubScene>(base));
    manager.push(std::make_unique<StubScene>(overlay, false, true));

    FrameBuffer buffer(20, 8);
    manager.draw(buffer, context);

    CHECK_EQ(base.draws, 1);
    CHECK_EQ(overlay.draws, 1);
}

DUNGEON_TEST(scene, an_opaque_scene_hides_scenes_below_it) {
    SceneManager manager;
    SceneContext context{manager};

    SceneStats base;
    SceneStats overlay;

    manager.push(std::make_unique<StubScene>(base));
    manager.push(std::make_unique<StubScene>(overlay));

    FrameBuffer buffer(20, 8);
    manager.draw(buffer, context);

    CHECK_EQ(base.draws, 0);
    CHECK_EQ(overlay.draws, 1);
}

DUNGEON_TEST(scene, scene_changes_from_input_are_applied_after_the_dispatch) {
    SceneManager manager;
    SceneContext context{manager};

    SceneStats next;

    manager.push(std::make_unique<ActionScene>([&next](SceneContext& scene_context) {
        scene_context.scenes.push(std::make_unique<StubScene>(next));
    }));

    CHECK(manager.handle_key(KeyEvent::from_character('x'), context));
    CHECK_EQ(manager.size(), 2U);
    CHECK_EQ(next.enters, 1);
}

DUNGEON_TEST(scene, title_scene_starts_on_enter_and_quits_on_escape) {
    SceneManager manager;
    SceneContext context{manager};

    int started = 0;

    TitleSceneConfig config;
    config.title    = "DUNGEON";
    config.prompt   = "Press ENTER to start";
    config.on_start = [&started](SceneContext& /*scene_context*/) { ++started; };

    manager.push(std::make_unique<TitleScene>(config));

    CHECK(!manager.handle_key(KeyEvent::from_character('x'), context));
    CHECK_EQ(started, 0);

    CHECK(manager.handle_key(KeyEvent::from_character('\n'), context));
    CHECK_EQ(started, 1);

    CHECK(manager.handle_key(KeyEvent::from_character('\r'), context));
    CHECK_EQ(started, 2);

    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Escape), context));
    CHECK(context.quit_requested);
}

DUNGEON_TEST(scene, title_scene_draws_its_title_and_prompt) {
    SceneManager manager;
    SceneContext context{manager};

    TitleSceneConfig config;
    config.title  = "DUNGEON";
    config.prompt = "Press ENTER to start";
    config.hint   = "Esc to quit";

    manager.push(std::make_unique<TitleScene>(config));

    FrameBuffer buffer(60, 20);
    manager.draw(buffer, context);

    CHECK(buffer_contains(buffer, "DUNGEON"));
    CHECK(buffer_contains(buffer, "Press ENTER to start"));
    CHECK(buffer_contains(buffer, "Esc to quit"));
}

DUNGEON_TEST(scene, pause_scene_navigates_and_runs_option_actions) {
    SceneManager manager;
    SceneContext context{manager};

    int resume = 0;
    int save   = 0;
    int quit   = 0;

    PauseSceneConfig config;
    config.title    = "PAUSED";
    config.controls = "Move: WASD";
    config.options  = {
        PauseOption{"Resume",
                    [&resume](SceneContext& scene_context) {
                        ++resume;
                        scene_context.scenes.pop();
                        return std::string{};
                    }},
        PauseOption{"Save game",
                    [&save](SceneContext& /*scene_context*/) {
                        ++save;
                        return std::string{"Game saved."};
                    }},
        PauseOption{"Quit",
                    [&quit](SceneContext& scene_context) {
                        ++quit;
                        scene_context.quit_requested = true;
                        return std::string{};
                    }},
    };

    manager.push(std::make_unique<PauseScene>(config));

    auto* pause = dynamic_cast<PauseScene*>(manager.top());
    REQUIRE(pause != nullptr);
    CHECK_EQ(pause->selected(), 0U);

    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Down), context));
    CHECK_EQ(pause->selected(), 1U);
    CHECK(manager.handle_key(KeyEvent::from_character('\n'), context));
    CHECK_EQ(save, 1);
    CHECK_EQ(pause->status(), std::string("Game saved."));

    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Down), context));
    CHECK_EQ(pause->selected(), 2U);
    CHECK(manager.handle_key(KeyEvent::from_character(' '), context));
    CHECK_EQ(quit, 1);
    CHECK(context.quit_requested);

    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Up), context));
    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Up), context));
    CHECK_EQ(pause->selected(), 0U);

    context.quit_requested = false;
    CHECK(manager.handle_key(KeyEvent::from_character('\n'), context));
    CHECK_EQ(resume, 1);
    CHECK(manager.empty());
}

DUNGEON_TEST(scene, pause_scene_resumes_on_escape) {
    SceneManager manager;
    SceneContext context{manager};

    SceneStats base;

    PauseSceneConfig config;
    config.title   = "PAUSED";
    config.options = {PauseOption{"Resume", [](SceneContext& scene_context) {
                                      scene_context.scenes.pop();
                                      return std::string{};
                                  }}};

    manager.push(std::make_unique<StubScene>(base));
    manager.push(std::make_unique<PauseScene>(config));

    CHECK(manager.handle_key(KeyEvent::from_kind(KeyKind::Escape), context));
    CHECK_EQ(manager.size(), 1U);
}

DUNGEON_TEST(scene, pause_scene_freezes_the_scene_below_and_draws_its_controls) {
    SceneManager manager;
    SceneContext context{manager};

    SceneStats base;

    PauseSceneConfig config;
    config.title    = "PAUSED";
    config.controls = "Move: WASD\nPause: Esc";
    config.options  = {PauseOption{"Resume",
                                  [](SceneContext& scene_context) {
                                      scene_context.scenes.pop();
                                      return std::string{};
                                  }},
                       PauseOption{"Quit", [](SceneContext& scene_context) {
                                      scene_context.quit_requested = true;
                                      return std::string{};
                                  }}};

    manager.push(std::make_unique<StubScene>(base));
    manager.push(std::make_unique<PauseScene>(config));

    manager.update(dungeon::scene::Duration{0}, context);
    CHECK_EQ(base.updates, 0);

    FrameBuffer buffer(100, 40);
    manager.draw(buffer, context);

    CHECK_EQ(base.draws, 1);
    CHECK(buffer_contains(buffer, "PAUSED"));
    CHECK(buffer_contains(buffer, "Move: WASD"));
    CHECK(buffer_contains(buffer, "Resume"));
    CHECK(buffer_contains(buffer, "Quit"));
}
