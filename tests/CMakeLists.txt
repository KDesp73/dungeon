add_executable(dungeon_tests
    test_harness.cpp
    test_main.cpp
    core/ticker_tests.cpp
    core/version_tests.cpp
    display/buffer_tests.cpp
    display/ui_tests.cpp
    game/game_tests.cpp
    map/map_tests.cpp
    scene/game_scene_tests.cpp
    scene/scene_tests.cpp
)

target_include_directories(dungeon_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dungeon_tests PRIVATE dungeon::core)

dungeon_set_warnings(dungeon_tests)
dungeon_enable_sanitizers(dungeon_tests)

# Every directory with tests becomes a CTest entry. The suite name matches the
# directory name and is passed to the test binary as a filter. Add new suites
# here as their directory appears.
foreach(suite IN ITEMS core display game map scene)
    add_test(NAME ${suite} COMMAND dungeon_tests --suite ${suite})
endforeach()
