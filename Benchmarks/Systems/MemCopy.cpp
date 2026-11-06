#include <stdint.h>
#include <stdio.h>

int main() {
    int64_t src[16];
    int64_t dst[16];
    for (int64_t i = 0; i < 16; i = i + 1) {
        src[i] = (i * 3 + 1) % 97;
    }
    for (int64_t rep = 0; rep < 200000; rep = rep + 1) {
        for (int64_t i = 0; i < 16; i = i + 1) {
            dst[i] = src[i];
        }
    }
    int64_t sum = 0;
    for (int64_t i = 0; i < 16; i = i + 1) {
        sum = sum + dst[i];
    }
    printf("%lld\n", (long long)sum);
    return 0;
}
