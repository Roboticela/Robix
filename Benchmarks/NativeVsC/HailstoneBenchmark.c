#include <stdio.h>
#include <stdint.h>

static int64_t ComputeHailstoneMax(int64_t startVal) {
    int64_t n = startVal;
    int64_t highest = n;
    while (n > 1) {
        if (n % 2 == 0) {
            n /= 2;
        } else {
            n = 3 * n + 1;
        }
        if (n > highest) {
            highest = n;
        }
    }
    return highest;
}

int main(void) {
    int64_t limit = 2000000;
    int64_t maxPeak = 0;
    int64_t i = 1;
    while (i <= limit) {
        int64_t peak = ComputeHailstoneMax(i);
        if (peak > maxPeak) {
            maxPeak = peak;
        }
        i += 1;
    }
    printf("Hailstone Peak: %lld\n", (long long)maxPeak);
    return 0;
}

