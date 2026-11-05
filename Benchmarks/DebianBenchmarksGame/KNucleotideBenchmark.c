#include <stdio.h>
#include <stdint.h>

static int64_t RunKNucleotide(int64_t n) {
    int64_t seed = 123456789;
    int64_t last = 0;

    int64_t c00 = 0, c01 = 0, c02 = 0, c03 = 0;
    int64_t c10 = 0, c11 = 0, c12 = 0, c13 = 0;
    int64_t c20 = 0, c21 = 0, c22 = 0, c23 = 0;
    int64_t c30 = 0, c31 = 0, c32 = 0, c33 = 0;

    int64_t i = 0;
    while (i < n) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t curr = (seed / 65536) % 4;

        int64_t pair = last * 4 + curr;
        if (pair == 0) c00 += 1;
        else if (pair == 1) c01 += 1;
        else if (pair == 2) c02 += 1;
        else if (pair == 3) c03 += 1;
        else if (pair == 4) c10 += 1;
        else if (pair == 5) c11 += 1;
        else if (pair == 6) c12 += 1;
        else if (pair == 7) c13 += 1;
        else if (pair == 8) c20 += 1;
        else if (pair == 9) c21 += 1;
        else if (pair == 10) c22 += 1;
        else if (pair == 11) c23 += 1;
        else if (pair == 12) c30 += 1;
        else if (pair == 13) c31 += 1;
        else if (pair == 14) c32 += 1;
        else c33 += 1;

        last = curr;
        i += 1;
    }

    int64_t checksum = (c00 * 1) + (c01 * 2) + (c02 * 3) + (c03 * 4) +
                       (c10 * 5) + (c11 * 6) + (c12 * 7) + (c13 * 8) +
                       (c20 * 9) + (c21 * 10) + (c22 * 11) + (c23 * 12) +
                       (c30 * 13) + (c31 * 14) + (c32 * 15) + (c33 * 16);
    return checksum;
}

int main(void) {
    int64_t n = 10000000;
    int64_t res = RunKNucleotide(n);
    printf("C K-Nucleotide Result:\n%lld\n", (long long)res);
    return 0;
}

