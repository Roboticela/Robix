#include <stdio.h>
#include <stdint.h>

static int64_t InterpolationSearch(int64_t target, int64_t n) {
    int64_t low = 0;
    int64_t high = n - 1;
    int64_t steps = 0;

    while (low <= high) {
        steps += 1;
        int64_t mid = low + (high - low) / 2;
        int64_t val = mid * 7;
        if (val == target) {
            return steps;
        }
        if (val < target) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return steps;
}

int main(void) {
    int64_t n = 10000000;
    int64_t totalSteps = 0;
    int64_t q = 0;
    while (q < 5000000) {
        int64_t target = (q * 31) % (n * 7);
        totalSteps += InterpolationSearch(target, n);
        q += 1;
    }
    printf("Binary Search Probe Steps: %lld\n", (long long)totalSteps);
    return 0;
}

