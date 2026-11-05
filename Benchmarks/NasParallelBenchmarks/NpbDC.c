#include <stdio.h>
#include <stdint.h>

static int64_t RunDC(int64_t numRecords) {
    int64_t seed = 577215664;
    int64_t checksum = 0;

    int64_t agg0 = 0, agg1 = 0, agg2 = 0, agg3 = 0;
    int64_t agg4 = 0, agg5 = 0, agg6 = 0, agg7 = 0;

    int64_t i = 0;
    while (i < numRecords) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t d0 = (seed / 65536) % 4;
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t d1 = (seed / 65536) % 2;
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t measure = (seed / 65536) % 100;

        int64_t slot = d0 * 2 + d1;
        if (slot < 4) {
            if (slot == 0) agg0 = (agg0 + measure) % 1000000007;
            else if (slot == 1) agg1 = (agg1 + measure) % 1000000007;
            else if (slot == 2) agg2 = (agg2 + measure) % 1000000007;
            else agg3 = (agg3 + measure) % 1000000007;
        } else {
            if (slot == 4) agg4 = (agg4 + measure) % 1000000007;
            else if (slot == 5) agg5 = (agg5 + measure) % 1000000007;
            else if (slot == 6) agg6 = (agg6 + measure) % 1000000007;
            else agg7 = (agg7 + measure) % 1000000007;
        }

        i += 1;
    }

    checksum = (agg0 * 1 + agg1 * 2 + agg2 * 3 + agg3 * 4 +
                agg4 * 5 + agg5 * 6 + agg6 * 7 + agg7 * 8) % 1000000007;

    return checksum;
}

int main(void) {
    int64_t n = 5000000;
    int64_t res = RunDC(n);
    printf("NPB DC Checksum:\n%lld\n", (long long)res);
    return 0;
}

