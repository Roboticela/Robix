#include <stdio.h>
#include <stdint.h>

static int64_t HornerPoly(int64_t x, int64_t degree) {
    int64_t result = 1;
    int64_t i = 0;
    while (i < degree) {
        result = (result * x + (i * 17 + 3)) % 1000000007;
        i = i + 1;
    }
    return result;
}

int main(void) {
    int64_t iterations = 1000000;
    int64_t sum = 0;
    int64_t x = 1;
    while (x <= iterations) {
        sum = (sum + HornerPoly(x % 100, 10)) % 1000000007;
        x = x + 1;
    }
    printf("Poly Evaluation Sum: %lld\n", (long long)sum);
    return 0;
}

