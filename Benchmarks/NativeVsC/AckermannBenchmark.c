#include <stdio.h>
#include <stdint.h>

static int64_t Ackermann(int64_t m, int64_t n) {
    if (m == 0) return n + 1;
    if (n == 0) return Ackermann(m - 1, 1);
    return Ackermann(m - 1, Ackermann(m, n - 1));
}

int main(void) {
    int64_t res = Ackermann(3, 11);
    printf("Ackermann(3, 11): %lld\n", (long long)res);
    return 0;
}

