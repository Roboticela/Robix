#include <iostream>
#include <cstdint>

static std::int64_t ComputeSteps(std::int64_t startVal) {
    std::int64_t n = startVal;
    std::int64_t steps = 0;
    while (n > 1) {
        if (n % 2 == 0) {
            n = n / 2;
        } else {
            n = 3 * n + 1;
        }
        steps += 1;
    }
    return steps;
}

int main() {
    std::int64_t limit = 2000000;
    std::int64_t maxSteps = 0;
    std::int64_t i = 1;
    while (i <= limit) {
        std::int64_t s = ComputeSteps(i);
        if (s > maxSteps) {
            maxSteps = s;
        }
        i += 1;
    }
    std::cout << "Max Collatz Steps: " << maxSteps << "\n";
    return 0;
}

