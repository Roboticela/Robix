#include <stdint.h>
#include <stdio.h>

int main(void) {
    int64_t sum = 0;
    for (int64_t i = 0; i < 20000000; i = i + 1) {
        sum = sum + (i % 1000);
    }
    printf("%lld\n", (long long)sum);
    return 0;
}
