cmake_minimum_required(VERSION 3.20)

project(dungeon
    VERSION 0.1.0
    DESCRIPTION "dungeon - a real-time terminal action RPG"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(DUNGEON_BUILD_TESTS "Build the unit test suite" ON)
option(DUNGEON_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(DUNGEON_SANITIZE "none" CACHE STRING
    "Sanitizers to enable: none, address, undefined, address,undefined, thread")
set_property(CACHE DUNGEON_SANITIZE PROPERTY STRINGS
    none address undefined address,undefined thread)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CompilerWarnings)
include(Sanitizers)

# ---------------------------------------------------------------------------
# dungeon_core: the game library. Everything that is not the process entry
# point lives here so that it can be linked into tests without a terminal.
# ---------------------------------------------------------------------------

add_library(dungeon_core STATIC
    src/core/version.cpp
)
add_library(dungeon::core ALIAS dungeon_core)

target_include_directories(dungeon_core
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(dungeon_core PUBLIC
    DUNGEON_VERSION_STRING="${PROJECT_VERSION}")

dungeon_set_warnings(dungeon_core)
dungeon_enable_sanitizers(dungeon_core)

# ---------------------------------------------------------------------------
# dungeon: the executable
# ---------------------------------------------------------------------------

add_executable(dungeon src/main.cpp)
target_link_libraries(dungeon PRIVATE dungeon::core)

dungeon_set_warnings(dungeon)
dungeon_enable_sanitizers(dungeon)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(DUNGEON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

include(GNUInstallDirs)
install(TARGETS dungeon RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/dungeon
        OPTIONAL)
