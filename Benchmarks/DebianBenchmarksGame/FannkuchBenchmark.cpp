#include <iostream>
#include <cstdint>

static inline int64_t Pow16(int64_t idx) {
    if (idx == 0) return 1;
    if (idx == 1) return 16;
    if (idx == 2) return 256;
    if (idx == 3) return 4096;
    if (idx == 4) return 65536;
    if (idx == 5) return 1048576;
    if (idx == 6) return 16777216;
    if (idx == 7) return 268435456;
    return 1;
}

static inline int64_t FlipCount(int64_t perm) {
    int64_t p = perm;
    int64_t first = p % 16;
    if (first == 1) return 0;
    int64_t flips = 0;
    while (first > 1) {
        int64_t lo = 0;
        int64_t hi = first - 1;
        while (lo < hi) {
            int64_t divLo = Pow16(lo);
            int64_t divHi = Pow16(hi);
            int64_t valLo = (p / divLo) % 16;
            int64_t valHi = (p / divHi) % 16;
            p = p - (valLo * divLo) - (valHi * divHi) + (valHi * divLo) + (valLo * divHi);
            lo = lo + 1;
            hi = hi - 1;
        }
        flips = flips + 1;
        first = p % 16;
    }
    return flips;
}

static inline int64_t MakePerm(int64_t r) {
    int64_t p = (((r + 7) % 8) + 1) +
                ((((r + 14) % 8) + 1) * 16) +
                ((((r + 21) % 8) + 1) * 256) +
                ((((r + 28) % 8) + 1) * 4096) +
                ((((r + 35) % 8) + 1) * 65536) +
                ((((r + 42) % 8) + 1) * 1048576) +
                ((((r + 49) % 8) + 1) * 16777216) +
                ((((r + 56) % 8) + 1) * 268435456);
    return p;
}

static int64_t RunFannkuch(int64_t n) {
    (void)n;
    int64_t maxFlips = 0;
    int64_t checksum = 0;
    int64_t count = 1000000;
    int64_t i = 0;
    int64_t seed = 123456789;
    while (i < count) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t r1 = seed % 40320;
        int64_t p1 = MakePerm(r1);
        int64_t flips1 = FlipCount(p1);
        if (flips1 > maxFlips) maxFlips = flips1;
        checksum += flips1;

        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t r2 = seed % 40320;
        int64_t p2 = MakePerm(r2);
        int64_t flips2 = FlipCount(p2);
        if (flips2 > maxFlips) maxFlips = flips2;
        checksum -= flips2;

        i += 2;
    }
    return checksum + maxFlips;
}

int main() {
    int64_t res = RunFannkuch(8);
    std::cout << "CPP Fannkuch-Redux Result:\n" << res << "\n";
    return 0;
}
