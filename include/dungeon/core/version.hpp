#pragma once

#include <string_view>

namespace dungeon::core {

#ifndef DUNGEON_VERSION_STRING
// Fallback for IDEs and ad-hoc compilations that do not define the version.
#define DUNGEON_VERSION_STRING "0.0.0-dev"
#endif

static_assert(__cplusplus >= 202002L, "dungeon requires C++20");

/// Human readable project name, as shown in the window title and the about screen.
inline constexpr std::string_view kProjectName = "dungeon";

/// Version of the game, injected by CMake from the project version.
inline constexpr std::string_view kVersion = DUNGEON_VERSION_STRING;

}  // namespace dungeon::core
