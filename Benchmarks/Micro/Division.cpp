#include <stdint.h>
#include <stdio.h>

int main() {
    int64_t sum = 0;
    for (int64_t i = 1; i <= 5000000; i = i + 1) {
        sum = sum + (i * 13) / 7;
    }
    printf("%lld\n", (long long)sum);
    return 0;
}
