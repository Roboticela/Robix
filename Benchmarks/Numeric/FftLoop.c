#include <stdint.h>
#include <stdio.h>

int main(void) {
    int64_t buf[16];
    for (int64_t i = 0; i < 16; i = i + 1) {
        buf[i] = (i * 9 + 1) % 10007;
    }
    for (int64_t rep = 0; rep < 200000; rep = rep + 1) {
        int64_t len = 1;
        while (len < 16) {
            int64_t i = 0;
            while (i < 16) {
                for (int64_t j = 0; j < len; j = j + 1) {
                    int64_t u = buf[i + j];
                    int64_t v = buf[i + j + len];
                    buf[i + j] = (u + v) % 10007;
                    int64_t diff = u - v;
                    if (diff < 0) {
                        diff = diff + 10007;
                    }
                    buf[i + j + len] = diff % 10007;
                }
                i = i + len + len;
            }
            len = len + len;
        }
    }
    int64_t sum = 0;
    for (int64_t i = 0; i < 16; i = i + 1) {
        sum = sum + buf[i];
    }
    printf("%lld\n", (long long)sum);
    return 0;
}
