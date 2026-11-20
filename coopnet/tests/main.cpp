#include "test_framework.h"

int main() {
    const auto& cases = coopnet_test::registry();
    if (cases.empty()) {
        std::printf("no tests registered\n");
        return 1;
    }

    int failed = 0;
    for (const auto& test : cases) {
        const int before = coopnet_test::failureCount();
        test.fn();
        const bool passed = coopnet_test::failureCount() == before;
        std::printf("[%s] %s\n", passed ? " ok " : "FAIL", test.name);
        std::fflush(stdout);
        if (!passed) {
            ++failed;
        }
    }

    std::printf("\n%zu tests, %d failed\n", cases.size(), failed);
    return failed == 0 ? 0 : 1;
}