#include "dungeon/scene/store_scene.hpp"

#include "dungeon/display/style.hpp"
#include "dungeon/display/ui.hpp"
#include "dungeon/scene/scene_manager.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace dungeon::scene {

namespace {

constexpr display::CellStyle kTitleStyle{display::Color::BrightYellow, display::Color::Default,
                                         display::TextStyle::Bold};
constexpr display::CellStyle kSelectedStyle{display::Color::Black, display::Color::BrightGreen,
                                            display::TextStyle::Bold};
constexpr display::CellStyle kSoldStyle{display::Color::BrightBlack, display::Color::Default,
                                        display::TextStyle::None};
constexpr display::CellStyle kUpgradeStyle{display::Color::BrightCyan, display::Color::Default,
                                           display::TextStyle::Bold};
constexpr display::CellStyle kUnaffordableStyle{display::Color::Red, display::Color::Default,
                                                display::TextStyle::None};
constexpr display::CellStyle kHintStyle{display::Color::BrightBlack, display::Color::Default,
                                        display::TextStyle::None};

constexpr std::size_t kNameColumn  = 14;
constexpr std::size_t kPriceColumn = 42;

std::string clip(std::string text, std::size_t width) {
    if (text.size() > width) {
        text.resize(width);
    }
    return text;
}

} // namespace

StoreScene::StoreScene(game::Game& game, StoreSceneConfig config)
    : _game(game), _config(std::move(config)) {}

void StoreScene::move_selection(int delta) noexcept {
    const std::size_t count = _game.store_items().size();

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

void StoreScene::buy() {
    if (_game.store_items().empty()) {
        return;
    }

    const bool bought = _game.buy_store_item(_selected);

    if (!_game.messages().empty()) {
        _status = _game.messages().back();
    } else if (bought) {
        _status = "Purchased!";
    }
}

void StoreScene::on_exit() {
    _game.close_store();
}

bool StoreScene::handle_key(const controls::KeyEvent& key, SceneContext& context) {
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
            buy();
            return true;
        }
        break;
    case controls::KeyKind::Escape:
        context.scenes.pop();
        return true;
    case controls::KeyKind::None:
    case controls::KeyKind::Left:
    case controls::KeyKind::Right:
        break;
    }

    return false;
}

void StoreScene::draw(display::FrameBuffer& buffer, SceneContext& /*context*/) {
    if (buffer.width() < 12 || buffer.height() < 8) {
        return;
    }

    const auto items = _game.store_items();

    const std::size_t panel_width  = std::min(buffer.width() - 2, std::size_t{60});
    const std::size_t panel_height = std::min(buffer.height() - 2, items.size() + 12);

    const display::tui::Rect frame{(buffer.width() - panel_width) / 2,
                                   (buffer.height() - panel_height) / 2, panel_width,
                                   panel_height};

    display::tui::fill(buffer, frame, U' ');
    display::tui::box(buffer, frame);
    display::tui::label(buffer, _config.title, frame.x + 2, frame.y, kTitleStyle);

    const std::size_t inner_width = frame.width > 4 ? frame.width - 4 : 0;
    const std::size_t left        = frame.x + 2;
    const std::size_t bottom      = frame.y + frame.height - 1;
    const std::size_t footer_row  = bottom - 1;
    std::size_t       row         = frame.y + 1;

    const auto line = [&](std::string_view text, display::CellStyle style = {}) {
        if (row >= footer_row) {
            return;
        }
        display::tui::label(buffer, clip(std::string(text), inner_width), left, row, style);
        ++row;
    };

    const auto centered = [&](std::string_view text, display::CellStyle style = {}) {
        if (row >= footer_row) {
            return;
        }
        display::tui::label_centered(buffer, clip(std::string(text), inner_width), frame, row,
                                     style);
        ++row;
    };

    const map::entities::Player* player = _game.player();
    const map::entities::Stats   stats =
        player != nullptr ? player->stats() : map::entities::Stats{};

    centered("Spend gold on permanent upgrades.", kHintStyle);
    ++row;

    line("Gold: " + std::to_string(stats.gold) + "    Level: " + std::to_string(stats.level) +
         "    HP: " + std::to_string(stats.hp) + "/" +
         std::to_string(map::entities::PLAYER_MAX_HP));
    line("Attack: " + std::to_string(stats.attack) + "    Defence: " +
         std::to_string(stats.defence) + "    Stamina: " + std::to_string(stats.stamina));

    ++row;

    for (std::size_t index = 0; index < items.size(); ++index) {
        if (row >= footer_row) {
            break;
        }

        const game::StoreItem& item     = items[index];
        const bool             selected = index == _selected;
        const bool             sold     = _game.store_item_sold(index);
        const bool             owned    = player != nullptr && player->has_item(item.kind);
        const bool             afford   = stats.gold >= item.price;

        std::string text = selected ? "> " : "  ";
        text += item.name;
        if (text.size() < kNameColumn) {
            text.append(kNameColumn - text.size(), ' ');
        }
        text += item.description;
        if (text.size() < kPriceColumn) {
            text.append(kPriceColumn - text.size(), ' ');
        }
        text += std::to_string(item.price) + "g";

        display::CellStyle style{};
        if (selected) {
            style = kSelectedStyle;
        } else if (sold) {
            style = kSoldStyle;
            text += "  [SOLD]";
        } else if (owned) {
            style = kUpgradeStyle;
            text += "  [UPGRADE]";
        } else if (!afford) {
            style = kUnaffordableStyle;
        }

        display::tui::label(buffer, clip(std::move(text), inner_width), left, row, style);
        ++row;
    }

    if (!_status.empty()) {
        ++row;
        line(_status, kHintStyle);
    }

    if (!_config.footer.empty() && footer_row > frame.y) {
        display::tui::label_centered(buffer, clip(_config.footer, inner_width), frame, footer_row,
                                     kHintStyle);
    }
}

} // namespace dungeon::scene
