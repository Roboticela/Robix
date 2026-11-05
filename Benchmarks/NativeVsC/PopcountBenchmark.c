#include <stdio.h>
#include <stdint.h>

static int64_t CountSetBits(int64_t n) {
    int64_t count = 0;
    while (n > 0) {
        count += (n % 2);
        n /= 2;
    }
    return count;
}

int main(void) {
    int64_t limit = 20000000;
    int64_t totalBits = 0;
    int64_t i = 1;
    while (i <= limit) {
        totalBits += CountSetBits(i);
        i += 1;
    }
    printf("Bit Count Total: %lld\n", (long long)totalBits);
    return 0;
}

