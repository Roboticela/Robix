#include <stdio.h>
#include <stdint.h>

static int64_t RunTradebeansMarket(int64_t numTrades) {
    int64_t b0 = 100, b1 = 99, b2 = 98, b3 = 97;
    int64_t s0 = 101, s1 = 102, s2 = 103, s3 = 104;

    int64_t seed = 555444333;
    int64_t totalExecutedVol = 0;
    int64_t totalTurnover = 0;

    int64_t i = 0;
    while (i < numTrades) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t orderSide = (seed / 65536) % 2;
        int64_t price = 95 + ((seed / 16384) % 12);
        int64_t qty = ((seed / 256) % 10) + 1;

        if (orderSide == 0) {
            if (price >= s0) {
                totalExecutedVol += qty;
                totalTurnover += qty * s0;
                if (totalTurnover >= 1000000007) {
                    totalTurnover -= 1000000007;
                }
                s0 += 1;
            } else {
                b0 = price;
            }
        } else {
            if (price <= b0) {
                totalExecutedVol += qty;
                totalTurnover += qty * b0;
                if (totalTurnover >= 1000000007) {
                    totalTurnover -= 1000000007;
                }
                b0 -= 1;
            } else {
                s0 = price;
            }
        }

        i += 1;
    }

    return (totalTurnover + totalExecutedVol + b0 + s0) % 1000000007;
}

int main(void) {
    int64_t n = 5000000;
    int64_t res = RunTradebeansMarket(n);
    printf("DaCapo Tradebeans Checksum:\n%lld\n", (long long)res);
    return 0;
}
