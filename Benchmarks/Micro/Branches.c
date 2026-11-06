#include <stdint.h>
#include <stdio.h>

int main(void) {
    int64_t acc = 0;
    for (int64_t i = 0; i < 15000000; i = i + 1) {
        int64_t m = i % 4;
        if (m == 0) {
            acc = acc + 1;
        } else if (m == 1) {
            acc = acc + 2;
        } else if (m == 2) {
            acc = acc + 3;
        } else {
            acc = acc + 4;
        }
    }
    printf("%lld\n", (long long)acc);
    return 0;
}
