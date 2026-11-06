#include <stdint.h>
#include <stdio.h>

static int64_t Mix(int64_t x) {
    return x * 3 + 1;
}

int main() {
    int64_t sum = 0;
    for (int64_t i = 0; i < 10000000; i = i + 1) {
        sum = sum + Mix(i);
    }
    printf("%lld\n", (long long)sum);
    return 0;
}
