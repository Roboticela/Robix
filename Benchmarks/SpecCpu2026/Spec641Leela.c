#include <stdio.h>
#include <stdint.h>

static int64_t RunRollout(int64_t seed) {
    int64_t s = seed;
    int64_t move = 0;
    int64_t blackStones = 0;
    int64_t whiteStones = 0;

    while (move < 24) {
        s = (s * 1103515245 + 12345) % 2147483648;
        int64_t liberty = (s / 65536) % 19;
        if (move % 2 == 0) {
            blackStones += liberty;
        } else {
            whiteStones += liberty;
        }
        move += 1;
    }

    return (blackStones >= whiteStones) ? 1 : 0;
}

static int64_t RunMCTSSim(int64_t numRollouts) {
    int64_t wins = 0;
    int64_t i = 0;
    while (i < numRollouts) {
        wins += RunRollout(i * 7919 + 101);
        i += 1;
    }
    return wins;
}

int main(void) {
    int64_t n = 2000000;
    int64_t res = RunMCTSSim(n);
    printf("SPEC 641.leela_r Checksum:\n%lld\n", (long long)res);
    return 0;
}

