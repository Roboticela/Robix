#include <stdio.h>
#include <stdint.h>

static int64_t RunBzip2(int64_t n) {
    int64_t seed = 543216789;
    int64_t checksum = 0;

    int64_t lastByte = -1;
    int64_t runLen = 0;
    int64_t bitBuffer = 0;
    int64_t bitCount = 0;

    int64_t i = 0;
    while (i < n) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t currByte = (seed / 65536) % 64;

        if (currByte == lastByte) {
            runLen += 1;
        } else {
            if (runLen > 0) {
                bitBuffer = (bitBuffer * 64 + lastByte) % 1000000007;
                bitCount += 6;
                if (bitCount >= 24) {
                    checksum = (checksum * 31 + bitBuffer) % 1000000007;
                    bitBuffer = 0;
                    bitCount = 0;
                }
            }
            lastByte = currByte;
            runLen = 1;
        }
        i += 1;
    }

    if (runLen > 0) {
        bitBuffer = (bitBuffer * 64 + lastByte) % 1000000007;
        checksum = (checksum * 31 + bitBuffer) % 1000000007;
    }
    return checksum;
}

int main(void) {
    int64_t n = 5000000;
    int64_t res = RunBzip2(n);
    printf("SPEC 602.gcc_r Checksum:\n%lld\n", (long long)res);
    return 0;
}

