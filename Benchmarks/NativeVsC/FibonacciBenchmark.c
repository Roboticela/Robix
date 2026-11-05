#include <stdio.h>
#include <stdint.h>

static int64_t ComputeFib(int64_t n) {
    int64_t a = 0;
    int64_t b = 1;
    int64_t i = 0;
    while (i < n) {
        int64_t temp = a + b;
        a = b;
        b = temp;
        i = i + 1;
    }
    return a;
}

int main(void) {
    int64_t iterations = 100000000;
    int64_t sum = 0;
    int64_t k = 0;
    while (k < iterations) {
        sum = sum + (k % 2);
        k = k + 1;
    }
    int64_t fib45 = ComputeFib(45);
    printf("C Native Fibonacci(45):\n");
    printf("%lld\n", (long long)fib45);
    printf("Checksum:\n");
    printf("%lld\n", (long long)sum);
    return 0;
}
