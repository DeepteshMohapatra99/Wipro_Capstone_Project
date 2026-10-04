// Minimal test framework (no external dependencies).
//
//   TEST(name) { CHECK(cond); CHECK_EQ(a, b); }
//   int main() { return runAllTests(); }

#ifndef TEST_FRAMEWORK_HPP
#define TEST_FRAMEWORK_HPP

#include <functional>
#include <iostream>
#include <string>
#include <vector>

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& testRegistry()
{
    static std::vector<TestCase> tests;
    return tests;
}

inline int& currentFailures()
{
    static int failures = 0;
    return failures;
}

struct TestRegistrar {
    TestRegistrar(const std::string& name, std::function<void()> fn)
    {
        testRegistry().push_back({name, std::move(fn)});
    }
};

#define TEST(name)                                              \
    static void name();                                         \
    static TestRegistrar registrar_##name(#name, name);         \
    static void name()

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::cout << "    FAILED: " #cond " (" __FILE__ ":" << __LINE__     \
                      << ")\n";                                                 \
            ++currentFailures();                                                \
        }                                                                       \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        auto va_ = (a);                                                         \
        auto vb_ = (b);                                                         \
        if (!(va_ == vb_)) {                                                    \
            std::cout << "    FAILED: " #a " == " #b " (got " << va_ << " vs "  \
                      << vb_ << ", " __FILE__ ":" << __LINE__ << ")\n";         \
            ++currentFailures();                                                \
        }                                                                       \
    } while (0)

#define CHECK_THROWS(expr)                                                      \
    do {                                                                        \
        bool thrown_ = false;                                                   \
        try {                                                                   \
            expr;                                                               \
        } catch (...) {                                                         \
            thrown_ = true;                                                     \
        }                                                                       \
        if (!thrown_) {                                                         \
            std::cout << "    FAILED: expected exception from " #expr " ("      \
                      << __FILE__ ":" << __LINE__ << ")\n";                     \
            ++currentFailures();                                                \
        }                                                                       \
    } while (0)

inline int runAllTests()
{
    int passed = 0;
    int failed = 0;

    for (const TestCase& t : testRegistry()) {
        currentFailures() = 0;
        try {
            t.fn();
        } catch (const std::exception& e) {
            std::cout << "    FAILED: unexpected exception: " << e.what() << '\n';
            ++currentFailures();
        }

        if (currentFailures() == 0) {
            std::cout << "[PASS] " << t.name << '\n';
            ++passed;
        } else {
            std::cout << "[FAIL] " << t.name << '\n';
            ++failed;
        }
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed, "
              << testRegistry().size() << " total\n";
    return failed == 0 ? 0 : 1;
}

#endif // TEST_FRAMEWORK_HPP
