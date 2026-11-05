#include <stdio.h>
#include <stdint.h>

static int64_t Accumulate(int64_t outer, int64_t inner) {
    int64_t total = 0;
    int64_t i = 0;
    while (i < outer) {
        int64_t j = 0;
        int64_t sub = 0;
        while (j < inner) {
            sub += (i * 3 + j * 5) % 97;
            j += 1;
        }
        total += sub;
        i += 1;
    }
    return total;
}

int main(void) {
    int64_t res = Accumulate(50000, 1000);
    printf("Nested Accum Total: %lld\n", (long long)res);
    return 0;
}

