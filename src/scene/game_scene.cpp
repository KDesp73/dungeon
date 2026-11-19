#include "dungeon/scene/game_scene.hpp"

#include "dungeon/display/ui.hpp"
#include "dungeon/scene/pause_scene.hpp"
#include "dungeon/scene/scene_manager.hpp"
#include "dungeon/scene/store_scene.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dungeon::scene {

namespace {

/// Draws a centred end-of-run panel with the title and @p lines.
void draw_result_panel(display::FrameBuffer& buffer, const display::tui::Layout& layout,
                       std::string_view title, const std::vector<std::string>& lines,
                       display::CellStyle title_style) {
    std::size_t content_width = display::tui::text_width(title);
    for (const std::string& line : lines) {
        content_width = std::max(content_width, display::tui::text_width(line));
    }

    const std::size_t panel_width  = std::min(layout.game.width, content_width + 4);
    const std::size_t panel_height = lines.size() + 6;

    if (layout.game.width > 4 && layout.game.height >= panel_height) {
        const display::tui::Rect panel{
            layout.game.x + (layout.game.width - panel_width) / 2,
            layout.game.y + (layout.game.height - panel_height) / 2,
            panel_width,
            panel_height,
        };

        display::tui::fill(buffer, panel);
        display::tui::box(buffer, panel);

        std::size_t row = panel.y + 1;
        display::tui::label_centered(buffer, title, panel, row, title_style);

        row += 2;
        for (const std::string& line : lines) {
            display::tui::label_centered(buffer, line, panel, row);
            ++row;
        }

        row += 1;
        display::tui::label_centered(buffer, "Press R to restart", panel, row);
        return;
    }

    const std::size_t row = layout.game.y + layout.game.height / 2;
    display::tui::label_centered(buffer, title, layout.game, row, title_style);
    display::tui::label_centered(buffer, "Press R to restart", layout.game, row + 1);
}

} // namespace

GameScene::GameScene(GameSceneConfig config) : _config(std::move(config)) {
    if (_config.layout) {
        _game.generate(*_config.layout);
        return;
    }

    if (_config.seed) {
        _game.map().reseed(*_config.seed);
    }

    _game.generate(_config.width, _config.height);
}

bool GameScene::handle_key(const controls::KeyEvent& key, SceneContext& context) {
    if (_game.game_over() || _game.victory()) {
        if (key.kind == controls::KeyKind::Character && (key.value == 'r' || key.value == 'R')) {
            _game.restart();
            return true;
        }

        if (key.kind == controls::KeyKind::Escape) {
            open_pause_menu(context);
            return true;
        }

        return false;
    }

    if (key.kind == controls::KeyKind::Escape) {
        open_pause_menu(context);
        return true;
    }

    if (const auto direction = game::direction_from_key(key)) {
        static_cast<void>(_game.move(*direction));
        update_store_proximity(context);
        return true;
    }

    if (key.kind != controls::KeyKind::Character) {
        return false;
    }

    switch (key.value) {
#ifdef DUNGEON_DEV
    case '@':
        jump_to_special_level(context);
        return true;
#endif
    case 'h':
    case 'H':
        static_cast<void>(_game.use_health_potion());
        return true;
    case '[':
        _game.cycle_chest_selection(-1);
        return true;
    case ']':
        _game.cycle_chest_selection(1);
        return true;
    case '\r':
    case '\n':
        static_cast<void>(_game.take_selected_chest_item());
        return true;
    default:
        if (key.value >= '1' && key.value <= '9') {
            _game.select_chest_item(static_cast<std::size_t>(key.value - '1'));
            return true;
        }
        return false;
    }
}

void GameScene::update(Duration delta, SceneContext& context) {
    if (_game.game_over() || _game.victory()) {
        return;
    }

    _game.update(std::chrono::duration<double>(delta).count());
    update_store_proximity(context);
}

void GameScene::draw(display::FrameBuffer& buffer, SceneContext& /*context*/) {
    const display::tui::Layout layout = display::tui::make_layout(buffer.width(), buffer.height());
    const display::tui::HudState hud{_game.messages(), _game.chest_selection(), _game.depth(),
                                     _game.run_stats().keys_collected,
                                     _game.run_stats().bosses_defeated};

    display::tui::draw_ui(buffer, _game.map(), layout, hud);

    if (!_game.game_over() && !_game.victory()) {
        return;
    }

    const game::RunStats& run    = _game.run_stats();
    const auto*           player = _game.player();
    const int             gold   = player != nullptr ? player->stats().gold : 0;
    const std::size_t     level  = player != nullptr ? player->stats().level : 0;

    std::vector<std::string> lines{
        "Dungeons completed: " + std::to_string(run.dungeons_completed),
        "Depth reached: " + std::to_string(_game.depth()),
        "Enemies killed: " + std::to_string(run.enemies_killed),
        "Bosses defeated: " + std::to_string(run.bosses_defeated),
        "Keys collected: " + std::to_string(run.keys_collected),
        "Gold collected: " + std::to_string(run.gold_collected),
        "Gold remaining: " + std::to_string(gold),
        "Level reached: " + std::to_string(level),
        "Potions used: " + std::to_string(run.potions_used),
    };

    if (_game.victory()) {
        draw_result_panel(buffer, layout, "VICTORY", lines,
                          display::CellStyle{display::Color::BrightGreen, display::Color::Default,
                                             display::TextStyle::Bold});
    } else {
        draw_result_panel(buffer, layout, "YOU DIED", lines,
                          display::CellStyle{display::Color::BrightRed, display::Color::Default,
                                             display::TextStyle::Bold});
    }
}

void GameScene::open_pause_menu(SceneContext& context) {
    PauseSceneConfig config;
    config.title    = _config.pause_title;
    config.controls = _config.pause_controls;
    config.options  = {
        PauseOption{"Resume",
                    [](SceneContext& menu_context) {
                        menu_context.scenes.pop();
                        return std::string{};
                    }},
        PauseOption{"Save game", [this](SceneContext& /*menu_context*/) { return save_game(); }},
        PauseOption{"Quit",
                    [](SceneContext& menu_context) {
                        menu_context.quit_requested = true;
                        return std::string{};
                    }},
    };

    context.scenes.push(std::make_unique<PauseScene>(std::move(config)));
}

std::string GameScene::save_game() {
    if (!_config.save_handler) {
        return "Saving is not available yet.";
    }

    return _config.save_handler(_game) ? "Game saved." : "Could not save the game.";
}

#ifdef DUNGEON_DEV
void GameScene::jump_to_special_level(SceneContext& context) {
    // Special levels are at most kBossInterval depths apart, and the final boss
    // is the end of the run, so never descend past it.
    for (std::size_t i = 0; i < game::kBossInterval; ++i) {
        if (_game.depth() >= game::kFinalBossDepth) {
            break;
        }

        _game.descend();

        if (_game.store_level() || _game.boss_level()) {
            break;
        }
    }

    // The player may have been next to a shopkeeper before the jump. Clear the
    // latch so a new store can open normally after the teleport.
    _store_near = false;
    update_store_proximity(context);
}
#endif

void GameScene::update_store_proximity(SceneContext& context) {
    const bool near = _game.nearby_shopkeeper() != nullptr;

    // Open the shop when the player enters the shopkeeper's range. The latch
    // keeps it closed after the player leaves the menu until they walk away and
    // come back.
    if (near && !_store_near) {
        _game.open_store();
        context.scenes.push(std::make_unique<StoreScene>(_game));
    }

    _store_near = near;
}

} // namespace dungeon::scene
