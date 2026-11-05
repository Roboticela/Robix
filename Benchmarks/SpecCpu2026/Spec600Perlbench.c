#include <stdio.h>
#include <stdint.h>

static int64_t RunCompress(int64_t n) {
    int64_t seed = 987654321;
    int64_t checksum = 0;

    int64_t h0 = 0, h1 = 0, h2 = 0, h3 = 0;
    int64_t h4 = 0, h5 = 0, h6 = 0, h7 = 0;

    int64_t i = 0;
    while (i < n) {
        seed = (seed * 1664525 + 1013904223) % 2147483648;
        int64_t byteVal = (seed / 65536) % 256;

        int64_t slot = (byteVal * 31 + i) % 8;
        if (slot == 0) { h0 = (h0 * 33 + byteVal) % 1000000007; checksum = (checksum + h0) % 1000000007; }
        else if (slot == 1) { h1 = (h1 * 33 + byteVal) % 1000000007; checksum = (checksum + h1) % 1000000007; }
        else if (slot == 2) { h2 = (h2 * 33 + byteVal) % 1000000007; checksum = (checksum + h2) % 1000000007; }
        else if (slot == 3) { h3 = (h3 * 33 + byteVal) % 1000000007; checksum = (checksum + h3) % 1000000007; }
        else if (slot == 4) { h4 = (h4 * 33 + byteVal) % 1000000007; checksum = (checksum + h4) % 1000000007; }
        else if (slot == 5) { h5 = (h5 * 33 + byteVal) % 1000000007; checksum = (checksum + h5) % 1000000007; }
        else if (slot == 6) { h6 = (h6 * 33 + byteVal) % 1000000007; checksum = (checksum + h6) % 1000000007; }
        else { h7 = (h7 * 33 + byteVal) % 1000000007; checksum = (checksum + h7) % 1000000007; }

        i += 1;
    }
    return checksum;
}

int main(void) {
    int64_t n = 4000000;
    int64_t res = RunCompress(n);
    printf("SPEC 600.perlbench_r Checksum:\n%lld\n", (long long)res);
    return 0;
}

