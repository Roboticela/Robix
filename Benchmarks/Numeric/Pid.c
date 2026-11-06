#include <stdint.h>
#include <stdio.h>

int main(void) {
    int64_t integral = 0;
    int64_t prev = 0;
    int64_t acc = 0;
    for (int64_t i = 0; i < 2000000; i = i + 1) {
        int64_t err = 100 - (i % 200);
        integral = integral + err;
        if (integral > 100000) { integral = 100000; }
        if (integral < 0 - 100000) { integral = 0 - 100000; }
        int64_t deriv = err - prev;
        int64_t step = 3 * err + integral / 8 + 2 * deriv;
        if (step < 0) { step = 0 - step; }
        acc = (acc + step) % 1000003;
        prev = err;
    }
    printf("%lld\n", (long long)acc);
    return 0;
}
