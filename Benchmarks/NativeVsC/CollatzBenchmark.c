#include <stdio.h>
#include <stdint.h>

static int64_t ComputeSteps(int64_t startVal) {
    int64_t n = startVal;
    int64_t steps = 0;
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

int main(void) {
    int64_t limit = 2000000;
    int64_t maxSteps = 0;
    int64_t i = 1;
    while (i <= limit) {
        int64_t s = ComputeSteps(i);
        if (s > maxSteps) {
            maxSteps = s;
        }
        i += 1;
    }
    printf("C Native Max Collatz Steps (1..2000000):\n");
    printf("%lld\n", (long long)maxSteps);
    return 0;
}

