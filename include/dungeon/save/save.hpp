#pragma once

#include "dungeon/game.hpp"

#include <string>
#include <utility>

namespace dungeon::save {

/// Default file used by the in-game Save/Load menu and the CLI.
inline constexpr const char* kDefaultSavePath = "dungeon.sav";

/// Result of a save or load operation.
struct SaveResult {
    bool        ok      = false;
    std::string message;

    [[nodiscard]] static SaveResult success(std::string message = {}) {
        return {true, std::move(message)};
    }

    [[nodiscard]] static SaveResult failure(std::string message) {
        return {false, std::move(message)};
    }
};

/// Writes the complete run to @p path using the binary save format.
///
/// The file is written to a temporary path and renamed into place, so a crash
/// during a save cannot truncate an existing good save. Callbacks such as
/// `Game::set_action_hook()` are not part of the saved state.
[[nodiscard]] SaveResult save_to_file(const game::Game& game,
                                      const std::string& path = kDefaultSavePath);

/// Replaces @p game with the run stored in @p path.
///
/// The game is only modified after the whole file has been validated. An
/// existing action hook is preserved; everything else comes from the save.
[[nodiscard]] SaveResult load_from_file(game::Game& game,
                                        const std::string& path = kDefaultSavePath);

/// True when @p path names an existing regular file.
[[nodiscard]] bool save_file_exists(const std::string& path = kDefaultSavePath);

} // namespace dungeon::save
