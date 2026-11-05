#include <stdio.h>
#include <stdint.h>

static int64_t Gcd(int64_t a, int64_t b) {
    while (b != 0) {
        int64_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

static int64_t Phi(int64_t n) {
    int64_t count = 0;
    int64_t i = 1;
    while (i <= n) {
        if (Gcd(i, n) == 1) {
            count += 1;
        }
        i += 1;
    }
    return count;
}

int main(void) {
    int64_t n = 3000;
    int64_t sumPhi = 0;
    int64_t k = 1;
    while (k <= n) {
        sumPhi += Phi(k);
        k += 1;
    }
    printf("Sum of Totients: %lld\n", (long long)sumPhi);
    return 0;
}

