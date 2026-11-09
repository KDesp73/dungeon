#include "dungeon/scene/game_scene.hpp"

#include "dungeon/display/ui.hpp"
#include "dungeon/scene/pause_scene.hpp"
#include "dungeon/scene/scene_manager.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace dungeon::scene {

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
    if (_game.game_over()) {
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
        return true;
    }

    if (key.kind != controls::KeyKind::Character) {
        return false;
    }

    switch (key.value) {
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

void GameScene::update(Duration delta, SceneContext& /*context*/) {
    if (_game.game_over()) {
        return;
    }

    _game.update(std::chrono::duration<double>(delta).count());
}

void GameScene::draw(display::FrameBuffer& buffer, SceneContext& /*context*/) {
    const display::tui::Layout layout = display::tui::make_layout(buffer.width(), buffer.height());
    const display::tui::HudState hud{_game.messages(), _game.chest_selection()};

    display::tui::draw_ui(buffer, _game.map(), layout, hud);

    if (_game.game_over()) {
        const std::size_t row    = layout.game.y + layout.game.height / 2;
        const display::CellStyle title{
            display::Color::BrightRed, display::Color::Default, display::TextStyle::Bold};

        display::tui::label_centered(buffer, "YOU DIED", layout.game, row, title);
        display::tui::label_centered(buffer, "Press R to restart", layout.game, row + 1);
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

} // namespace dungeon::scene
