#include <iostream>
#include <cstdint>

static int64_t RunIS(int64_t numKeys) {
    int64_t seed = 314159265;
    int64_t checksum = 0;

    int64_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;
    int64_t b4 = 0, b5 = 0, b6 = 0, b7 = 0;
    int64_t b8 = 0, b9 = 0, b10 = 0, b11 = 0;
    int64_t b12 = 0, b13 = 0, b14 = 0, b15 = 0;

    int64_t i = 0;
    while (i < numKeys) {
        seed = (seed * 1220703125 + 1013904223) % 2147483648;
        int64_t key = (seed / 65536) % 16;

        if (key < 8) {
            if (key == 0) b0 += 1;
            else if (key == 1) b1 += 1;
            else if (key == 2) b2 += 1;
            else if (key == 3) b3 += 1;
            else if (key == 4) b4 += 1;
            else if (key == 5) b5 += 1;
            else if (key == 6) b6 += 1;
            else b7 += 1;
        } else {
            if (key == 8) b8 += 1;
            else if (key == 9) b9 += 1;
            else if (key == 10) b10 += 1;
            else if (key == 11) b11 += 1;
            else if (key == 12) b12 += 1;
            else if (key == 13) b13 += 1;
            else if (key == 14) b14 += 1;
            else b15 += 1;
        }

        i += 1;
    }

    checksum = b0 * 1 + b1 * 2 + b2 * 3 + b3 * 4 +
               b4 * 5 + b5 * 6 + b6 * 7 + b7 * 8 +
               b8 * 9 + b9 * 10 + b10 * 11 + b11 * 12 +
               b12 * 13 + b13 * 14 + b14 * 15 + b15 * 16;

    return checksum;
}

int main() {
    int64_t n = 10000000;
    int64_t res = RunIS(n);
    std::cout << "NPB IS Checksum:\n" << res << "\n";
    return 0;
}

