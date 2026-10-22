#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace dungeon::test {

/// Thrown by REQUIRE when the checked expression failed.
class AssertionFailure {
public:
    explicit AssertionFailure(std::string message) : message_(std::move(message)) {}
    [[nodiscard]] const std::string& message() const noexcept { return message_; }

private:
    std::string message_;
};

/// A registered test case.
struct TestCase {
    std::string suite;
    std::string name;
    void (*function)();
};

/// Global registry of all test cases.
[[nodiscard]] std::vector<TestCase>& registry();

/// Registers a test case at static initialisation time.
struct Registrar {
    Registrar(std::string suite, std::string name, void (*function)());
};

/// Runs the registered test cases and returns a process exit code.
///
/// Usage: dungeon_tests [--suite <name>] [--list]
[[nodiscard]] int run_all(int argc, char** argv);

/// Records a failed expectation without aborting the test case.
void report_failure(std::string_view file, int line, std::string_view expression,
                    std::string_view detail);

/// Renders a value for the failure message.
template <typename T>
[[nodiscard]] std::string describe(const T& value) {
    std::ostringstream out;
    if constexpr (requires(std::ostringstream& stream, const T& item) { stream << item; }) {
        out << value;
    } else {
        out << "<unprintable>";
    }
    return out.str();
}

/// Checks that @p condition holds.
inline void check(bool condition, std::string_view expression, std::string_view file, int line) {
    if (!condition) {
        report_failure(file, line, expression, "expectation failed");
    }
}

/// Checks that two values are equal.
template <typename Left, typename Right>
void check_equal(const Left& left, const Right& right, std::string_view expression,
                 std::string_view file, int line) {
    bool equal = false;
    if constexpr (std::is_convertible_v<const Left&, std::string_view> &&
                  std::is_convertible_v<const Right&, std::string_view>) {
        // Compare the contents of strings instead of their addresses.
        equal = std::string_view(left) == std::string_view(right);
    } else {
        equal = left == right;
    }

    if (!equal) {
        report_failure(file, line, expression,
                       "expected " + describe(right) + " but got " + describe(left));
    }
}

/// Checks that @p condition holds and aborts the test case when it does not.
inline void require(bool condition, std::string_view expression, std::string_view file, int line) {
    if (!condition) {
        report_failure(file, line, expression, "requirement failed");
        throw AssertionFailure(std::string(expression));
    }
}

/// Checks that the expression throws @p exception_type.
template <typename Exception, typename Callable>
void check_throws_as(Callable&& callable, std::string_view expression, std::string_view file,
                     int line) {
    try {
        callable();
    } catch (const Exception&) {
        return;
    } catch (...) {
        report_failure(file, line, expression, "expected a different exception type");
        return;
    }
    report_failure(file, line, expression, "expected an exception but none was thrown");
}

}  // namespace dungeon::test

/// Defines a test case inside the suite @p suite.
#define DUNGEON_TEST(suite, name)                                                             \
    static void dungeon_test_##suite##_##name();                                              \
    [[maybe_unused]] static const ::dungeon::test::Registrar dungeon_registrar_##suite##_##name{ \
        #suite, #name, &dungeon_test_##suite##_##name};                                       \
    static void dungeon_test_##suite##_##name()

/// Checks that the expression is true; the test continues when it is not.
#define CHECK(expression) \
    ::dungeon::test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

/// Checks that two values are equal; the test continues when they are not.
#define CHECK_EQ(left, right) \
    ::dungeon::test::check_equal((left), (right), #left " == " #right, __FILE__, __LINE__)

/// Checks that the expression is true and aborts the test case when it is not.
#define REQUIRE(expression) \
    ::dungeon::test::require(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

/// Checks that the expression throws the given exception type.
#define CHECK_THROWS_AS(expression, exception_type)                                         \
    ::dungeon::test::check_throws_as<exception_type>([&]() { (expression); }, #expression,   \
                                                      __FILE__, __LINE__)
