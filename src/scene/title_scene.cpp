#include "dungeon/scene/title_scene.hpp"

#include "dungeon/display/style.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/scene/scene_manager.hpp"

#include <cstddef>
#include <utility>

namespace dungeon::scene {

namespace {

constexpr display::CellStyle kTitleStyle{display::Color::BrightYellow, display::Color::Default,
                                         display::TextStyle::Bold};
constexpr display::CellStyle kSubtitleStyle{display::Color::BrightBlack, display::Color::Default,
                                            display::TextStyle::None};
constexpr display::CellStyle kPromptStyle{display::Color::BrightGreen, display::Color::Default,
                                          display::TextStyle::Bold};
constexpr display::CellStyle kHintStyle{display::Color::BrightBlack, display::Color::Default,
                                        display::TextStyle::None};

bool is_enter(const controls::KeyEvent& key) noexcept {
    return key.kind == controls::KeyKind::Character && (key.value == '\r' || key.value == '\n');
}

} // namespace

TitleScene::TitleScene(TitleSceneConfig config) : _config(std::move(config)) {}

bool TitleScene::starts_game(const controls::KeyEvent& key) const noexcept {
    if (_config.start_key.is_none()) {
        return false;
    }

    if (key == _config.start_key) {
        return true;
    }

    return is_enter(key) && is_enter(_config.start_key);
}

bool TitleScene::handle_key(const controls::KeyEvent& key, SceneContext& context) {
    if (starts_game(key)) {
        if (_config.on_start) {
            _config.on_start(context);
        }
        return true;
    }

    if (_config.quit_on_escape && key.kind == controls::KeyKind::Escape) {
        context.quit_requested = true;
        return true;
    }

    return false;
}

void TitleScene::draw(display::FrameBuffer& buffer, SceneContext& /*context*/) {
    if (buffer.width() == 0 || buffer.height() == 0) {
        return;
    }

    const display::tui::Rect screen{0, 0, buffer.width(), buffer.height()};

    if (buffer.width() > 2 && buffer.height() > 2) {
        display::tui::box(buffer,
                          display::tui::Rect{1, 1, buffer.width() - 2, buffer.height() - 2});
    }

    const std::size_t title_y = buffer.height() / 3;
    display::tui::label_centered(buffer, _config.title, screen, title_y, kTitleStyle);

    if (!_config.subtitle.empty() && title_y + 2 < buffer.height()) {
        display::tui::label_centered(buffer, _config.subtitle, screen, title_y + 2, kSubtitleStyle);
    }

    if (!_config.prompt.empty()) {
        const std::size_t prompt_y = (buffer.height() * 3) / 4;
        if (prompt_y < buffer.height()) {
            display::tui::label_centered(buffer, _config.prompt, screen, prompt_y, kPromptStyle);
        }
    }

    if (!_config.hint.empty() && buffer.height() > 2) {
        const std::size_t hint_y = (buffer.height() * 3) / 4 + 2;
        if (hint_y < buffer.height() - 1) {
            display::tui::label_centered(buffer, _config.hint, screen, hint_y, kHintStyle);
        }
    }
}

} // namespace dungeon::scene
