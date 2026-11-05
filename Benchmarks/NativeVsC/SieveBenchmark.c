#include <stdio.h>
#include <stdint.h>

static int64_t SieveCount(int64_t limit) {
    int64_t count = 1;
    int64_t num = 3;
    while (num <= limit) {
        int64_t isPrime = 1;
        int64_t d = 3;
        while (d * d <= num) {
            if (num % d == 0) {
                isPrime = 0;
                break;
            }
            d += 2;
        }
        if (isPrime) {
            count += 1;
        }
        num += 2;
    }
    return count;
}

int main(void) {
    int64_t limit = 200000;
    int64_t total = SieveCount(limit);
    printf("Fast Prime Sieve: %lld\n", (long long)total);
    return 0;
}
