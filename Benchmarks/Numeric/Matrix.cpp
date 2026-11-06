#include <stdint.h>
#include <stdio.h>

int main() {
    int64_t a[16];
    int64_t b[16];
    for (int64_t i = 0; i < 16; i = i + 1) {
        a[i] = (i * 3 + 1) % 97;
        b[i] = (i * 5 + 2) % 89;
    }
    int64_t total = 0;
    for (int64_t rep = 0; rep < 30000; rep = rep + 1) {
        for (int64_t row = 0; row < 4; row = row + 1) {
            for (int64_t col = 0; col < 4; col = col + 1) {
                int64_t s = 0;
                for (int64_t k = 0; k < 4; k = k + 1) {
                    s = s + a[row * 4 + k] * b[k * 4 + col];
                }
                total = total + s;
            }
        }
    }
    printf("%lld\n", (long long)total);
    return 0;
}
