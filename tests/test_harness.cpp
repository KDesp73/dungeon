#include "test_harness.hpp"

#include <cstdio>
#include <exception>
#include <string>

namespace dungeon::test {
namespace {

std::size_t g_failures = 0;

std::string to_string(std::string_view value) { return std::string(value); }

}  // namespace

std::vector<TestCase>& registry() {
    static std::vector<TestCase> instance;
    return instance;
}

Registrar::Registrar(std::string suite, std::string name, void (*function)()) {
    registry().push_back(TestCase{std::move(suite), std::move(name), function});
}

void report_failure(std::string_view file, int line, std::string_view expression,
                    std::string_view detail) {
    ++g_failures;
    std::printf("    %s:%d: %s (%s)\n", to_string(file).c_str(), line,
                to_string(expression).c_str(), to_string(detail).c_str());
}

int run_all(int argc, char** argv) {
    std::string suite_filter;
    bool list_only = false;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--list") {
            list_only = true;
        } else if (argument == "--suite" && index + 1 < argc) {
            suite_filter = argv[++index];
        } else {
            std::printf("unknown argument: %s\n", argv[index]);
            std::printf("usage: dungeon_tests [--suite <name>] [--list]\n");
            return 2;
        }
    }

    std::size_t executed = 0;
    std::size_t failed = 0;

    for (const TestCase& test : registry()) {
        if (!suite_filter.empty() && test.suite != suite_filter) {
            continue;
        }

        if (list_only) {
            std::printf("%s.%s\n", test.suite.c_str(), test.name.c_str());
            continue;
        }

        const std::size_t failures_before = g_failures;
        std::printf("[ RUN      ] %s.%s\n", test.suite.c_str(), test.name.c_str());

        try {
            test.function();
        } catch (const AssertionFailure&) {
            // The failure was already reported by require().
        } catch (const std::exception& error) {
            report_failure(test.name, 0, "unexpected exception", error.what());
        } catch (...) {
            report_failure(test.name, 0, "unexpected exception", "unknown exception");
        }

        ++executed;
        if (g_failures > failures_before) {
            ++failed;
            std::printf("[   FAILED ] %s.%s\n", test.suite.c_str(), test.name.c_str());
        } else {
            std::printf("[       OK ] %s.%s\n", test.suite.c_str(), test.name.c_str());
        }
    }

    if (list_only) {
        return 0;
    }

    std::printf("\n%zu test(s) run, %zu failed, %zu check(s) failed\n", executed, failed, g_failures);
    return failed == 0 ? 0 : 1;
}

}  // namespace dungeon::test
