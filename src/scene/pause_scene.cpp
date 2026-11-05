#include "dungeon/scene/pause_scene.hpp"

#include "dungeon/display/style.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/scene/scene_manager.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dungeon::scene {

namespace {

constexpr std::size_t kPadding = 2;

constexpr display::CellStyle kTitleStyle{display::Color::BrightYellow, display::Color::Default,
                                         display::TextStyle::Bold};
constexpr display::CellStyle kSelectedStyle{display::Color::Black, display::Color::BrightGreen,
                                            display::TextStyle::Bold};
constexpr display::CellStyle kStatusStyle{display::Color::BrightGreen, display::Color::Default,
                                          display::TextStyle::None};
constexpr display::CellStyle kHintStyle{display::Color::BrightBlack, display::Color::Default,
                                        display::TextStyle::None};

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;

    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::size_t length =
            end == std::string_view::npos ? text.size() - start : end - start;
        lines.push_back(text.substr(start, length));

        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    return lines;
}

} // namespace

PauseScene::PauseScene(PauseSceneConfig config) : _config(std::move(config)) {}

void PauseScene::move_selection(int delta) noexcept {
    const std::size_t count = _config.options.size();
    if (count == 0) {
        _selected = 0;
        return;
    }

    if (_selected >= count) {
        _selected = 0;
    }

    if (delta > 0) {
        _selected = (_selected + 1) % count;
    } else if (delta < 0) {
        _selected = (_selected + count - 1) % count;
    }
}

void PauseScene::activate(SceneContext& context) {
    if (_selected >= _config.options.size()) {
        _selected = 0;
    }

    if (_config.options.empty()) {
        return;
    }

    PauseOption& option = _config.options[_selected];
    if (!option.action) {
        return;
    }

    std::string result = option.action(context);
    if (!result.empty()) {
        _status = std::move(result);
    }
}

bool PauseScene::handle_key(const controls::KeyEvent& key, SceneContext& context) {
    switch (key.kind) {
    case controls::KeyKind::Up:
        move_selection(-1);
        return true;
    case controls::KeyKind::Down:
        move_selection(1);
        return true;
    case controls::KeyKind::Character:
        if (key.value == 'w' || key.value == 'W') {
            move_selection(-1);
            return true;
        }
        if (key.value == 's' || key.value == 'S') {
            move_selection(1);
            return true;
        }
        if (key.value == '\r' || key.value == '\n' || key.value == ' ') {
            activate(context);
            return true;
        }
        break;
    case controls::KeyKind::Escape:
        if (_config.resume_on_escape) {
            context.scenes.pop();
            return true;
        }
        return false;
    case controls::KeyKind::None:
    case controls::KeyKind::Left:
    case controls::KeyKind::Right:
        break;
    }

    return false;
}

void PauseScene::draw(display::FrameBuffer& buffer, SceneContext& /*context*/) {
    if (buffer.width() < 4 || buffer.height() < 4) {
        return;
    }

    const std::vector<std::string_view> controls = split_lines(_config.controls);

    std::size_t content_width = display::tui::text_width(_config.title);
    for (const std::string_view line : controls) {
        content_width = std::max(content_width, display::tui::text_width(line));
    }
    for (const PauseOption& option : _config.options) {
        content_width = std::max(content_width, display::tui::text_width(option.label) + 2);
    }
    content_width = std::max(content_width, display::tui::text_width(_config.footer));
    content_width = std::max(content_width, display::tui::text_width(_status));

    std::size_t content_height = 1 + 1 + controls.size() + 1 + _config.options.size();
    if (!_status.empty()) {
        content_height += 2;
    }
    if (!_config.footer.empty()) {
        content_height += 2;
    }

    const std::size_t panel_width  = std::min(buffer.width(), content_width + 2 * kPadding + 2);
    const std::size_t panel_height = std::min(buffer.height(), content_height + 2);

    const display::tui::Rect panel{(buffer.width() - panel_width) / 2,
                                   (buffer.height() - panel_height) / 2, panel_width, panel_height};

    display::tui::fill(buffer, panel, U' ');
    display::tui::box(buffer, panel);

    const display::tui::Rect inner{panel.x + 1, panel.y + 1, panel.width - 2, panel.height - 2};
    const std::size_t        bottom = panel.y + panel.height - 1;
    std::size_t              row    = panel.y + 1;

    const auto centered_line = [&](std::string_view text, display::CellStyle style) {
        if (row >= bottom) {
            return;
        }
        display::tui::label_centered(buffer, text, inner, row, style);
        ++row;
    };

    centered_line(_config.title, kTitleStyle);
    ++row;

    std::size_t controls_width = 0;
    for (const std::string_view line : controls) {
        controls_width = std::max(controls_width, display::tui::text_width(line));
    }

    const std::size_t controls_x =
        inner.x + (inner.width > controls_width ? (inner.width - controls_width) / 2 : 0);

    for (const std::string_view line : controls) {
        if (row >= bottom) {
            break;
        }
        display::tui::label(buffer, line, controls_x, row, display::CellStyle{});
        ++row;
    }

    ++row;

    std::size_t option_width = 0;
    for (const PauseOption& option : _config.options) {
        option_width = std::max(option_width, display::tui::text_width(option.label) + 2);
    }

    const std::size_t option_x =
        inner.x + (inner.width > option_width ? (inner.width - option_width) / 2 : 0);

    for (std::size_t index = 0; index < _config.options.size(); ++index) {
        if (row >= bottom) {
            break;
        }

        const PauseOption& option   = _config.options[index];
        const bool         selected = index == _selected;
        const auto         style    = selected ? kSelectedStyle : display::CellStyle{};

        buffer.draw(option_x, row, selected ? U'>' : U' ', style);
        display::tui::label(buffer, option.label, option_x + 2, row, style);
        ++row;
    }

    if (!_status.empty()) {
        ++row;
        centered_line(_status, kStatusStyle);
    }

    if (!_config.footer.empty()) {
        ++row;
        centered_line(_config.footer, kHintStyle);
    }
}

} // namespace dungeon::scene
