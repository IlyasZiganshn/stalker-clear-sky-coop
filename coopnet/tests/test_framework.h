#pragma once

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace coopnet_test {

using TestFn = void (*)();

struct TestCase {
    const char* name;
    TestFn fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

inline int& failureCount() {
    static int count = 0;
    return count;
}

inline void reportFailure(const char* file, int line, const std::string& message) {
    ++failureCount();
    std::printf("    %s:%d: %s\n", file, line, message.c_str());
    std::fflush(stdout);
}

struct Registrar {
    Registrar(const char* name, TestFn fn) { registry().push_back({name, fn}); }
};

}  // namespace coopnet_test

#define COOP_TEST(name)                                                       \
    static void coop_test_##name();                                           \
    static ::coopnet_test::Registrar coop_test_reg_##name(#name,              \
                                                          &coop_test_##name); \
    static void coop_test_##name()

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            ::coopnet_test::reportFailure(__FILE__, __LINE__,                 \
                                          "CHECK failed: " #condition);       \
        }                                                                     \
    } while (0)

#define CHECK_EQ(actual, expected)                                            \
    do {                                                                      \
        const auto coop_a = (actual);                                         \
        const auto coop_e = (expected);                                       \
        if (!(coop_a == coop_e)) {                                            \
            std::ostringstream coop_msg;                                      \
            coop_msg << "CHECK_EQ failed: " #actual " == " #expected          \
                     << " (got " << +coop_a << ", expected " << +coop_e       \
                     << ")";                                                 \
            ::coopnet_test::reportFailure(__FILE__, __LINE__, coop_msg.str());\
        }                                                                     \
    } while (0)

#define CHECK_NEAR(actual, expected, eps)                                     \
    do {                                                                      \
        const double coop_a = static_cast<double>(actual);                    \
        const double coop_e = static_cast<double>(expected);                  \
        if (!(std::fabs(coop_a - coop_e) <= static_cast<double>(eps))) {      \
            std::ostringstream coop_msg;                                      \
            coop_msg << "CHECK_NEAR failed: " #actual " ~= " #expected        \
                     << " (got " << coop_a << ", expected " << coop_e         \
                     << ", eps " << static_cast<double>(eps) << ")";          \
            ::coopnet_test::reportFailure(__FILE__, __LINE__, coop_msg.str());\
        }                                                                     \
    } while (0)