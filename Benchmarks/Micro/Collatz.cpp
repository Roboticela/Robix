#include <stdint.h>
#include <stdio.h>

static int64_t Steps(int64_t n) {
    int64_t current = n;
    int64_t steps = 0;
    while (current != 1) {
        if (current % 2 == 0) {
            current = current / 2;
        } else {
            current = 3 * current + 1;
        }
        steps = steps + 1;
        if (steps > 400) {
            return steps;
        }
    }
    return steps;
}

int main() {
    int64_t sum = 0;
    for (int64_t n = 1; n <= 20000; n = n + 1) {
        sum = sum + Steps(n);
    }
    printf("%lld\n", (long long)sum);
    return 0;
}
