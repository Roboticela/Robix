#include <stdio.h>
#include <stdint.h>

static int64_t MatMulFlat(int64_t n) {
    int64_t sum = 0;
    int64_t i = 0;
    while (i < n) {
        int64_t j = 0;
        while (j < n) {
            int64_t k = 0;
            while (k < n) {
                int64_t aVal = (i * 37 + k) % 100;
                int64_t bVal = (k * 19 + j) % 100;
                sum = sum + aVal * bVal;
                k = k + 1;
            }
            j = j + 1;
        }
        i = i + 1;
    }
    return sum;
}

int main(void) {
    int64_t size = 250;
    int64_t total = MatMulFlat(size);
    printf("Matrix Multiply Sum: %lld\n", (long long)total);
    return 0;
}

