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

if(EMSCRIPTEN)
    # Sanitizers are generally not supported under Emscripten
    set(DUNGEON_SANITIZE "none" CACHE STRING "" FORCE)

    # Tell Emscripten to output an HTML wrapper page for testing
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    # Optional: Emscripten link flags (e.g., memory growth, preloading assets)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

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

# Collect every implementation file under src/, except the executable entry
# point. CONFIGURE_DEPENDS makes the build system notice added or renamed files
# without a manual configure step (supported by Ninja and Makefile generators).
file(GLOB_RECURSE DUNGEON_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM DUNGEON_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

if(NOT DUNGEON_CORE_SOURCES)
    message(FATAL_ERROR "No game sources found under ${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

add_library(dungeon_core STATIC ${DUNGEON_CORE_SOURCES})
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

# CLI11 provides the command line parser. Prefer a system package when one is
# available; otherwise CMake fetches a pinned release.
find_package(CLI11 CONFIG QUIET)
if(NOT CLI11_FOUND)
    include(FetchContent)

    set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CLI11_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(CLI11_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.7.2
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(CLI11)
endif()

add_executable(dungeon src/main.cpp)
target_link_libraries(dungeon PRIVATE dungeon::core CLI11::CLI11)

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
