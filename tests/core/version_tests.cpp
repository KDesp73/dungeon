#include "test_harness.hpp"

#include "dungeon/core/version.hpp"

DUNGEON_TEST(core, the_project_reports_a_name_and_a_version) {
    CHECK(!dungeon::core::kProjectName.empty());
    CHECK(!dungeon::core::kVersion.empty());
}
