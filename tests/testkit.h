#pragma once
#include <cstdio>
#include <string>
#include <vector>

// A very small test library, written here so the project needs no other.
//
//     TEST(json, parses_nested_arrays) {
//         CHECK(something);
//         CHECK_EQ(value, expected);
//     }
//
// The first word of TEST must match the file name: json_test.cpp uses TEST(json, ...).
namespace testkit {

struct Test {
    std::string name;
    void (*run)();
};

inline std::vector<Test>& allTests()
{
    static std::vector<Test> tests;
    return tests;
}

inline int& failures()
{
    static int count = 0;
    return count;
}

inline bool add(const char* suite, const char* name, void (*run)())
{
    allTests().push_back({std::string(suite) + "." + name, run});
    return true;
}

inline void fail(const char* file, int line, const char* message)
{
    std::printf("    FAILED %s:%d: %s\n", file, line, message);
    ++failures();
}

inline std::string show(const std::string& s) { return "\"" + s + "\""; }
inline std::string show(const char* s) { return show(std::string(s)); }
inline std::string show(bool b) { return b ? "true" : "false"; }
template <typename T>
std::string show(const T& v) { return std::to_string(v); }

} // namespace testkit

#define TEST(suite, name)                                                                     \
    static void suite##_##name();                                                             \
    [[maybe_unused]] static const bool suite##_##name##_added =                               \
        testkit::add(#suite, #name, suite##_##name);                                          \
    static void suite##_##name()

#define CHECK(condition)                                                                      \
    do {                                                                                      \
        if (!(condition)) testkit::fail(__FILE__, __LINE__, #condition);                      \
    } while (false)

#define CHECK_EQ(value, expected)                                                             \
    do {                                                                                      \
        const auto testValue = (value);              /* copies: `value` may be a temporary */  \
        const auto testExpected = (expected);                                                 \
        if (!(testValue == testExpected)) {                                                   \
            std::printf("    got %s, expected %s\n", testkit::show(testValue).c_str(),        \
                        testkit::show(testExpected).c_str());                                 \
            testkit::fail(__FILE__, __LINE__, #value " == " #expected);                       \
        }                                                                                     \
    } while (false)
