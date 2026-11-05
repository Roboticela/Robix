#include <stdio.h>
#include <stdint.h>

static int64_t ComputeBitPermutations(int64_t n) {
    int64_t acc = 123456789;
    int64_t i = 0;
    while (i < n) {
        acc = acc * 6364136223846793005ULL + 1442695040888963407ULL;
        acc = (acc * 33) + (i % 256);
        i += 1;
    }
    return acc;
}

int main(void) {
    int64_t n = 100000000;
    int64_t res = ComputeBitPermutations(n);
    printf("BitOps LCG Result: %lld\n", (long long)res);
    return 0;
}

