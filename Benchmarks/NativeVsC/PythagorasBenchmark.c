#include <stdio.h>
#include <stdint.h>

static int64_t Solve(int64_t limit) {
    int64_t sum = 0;
    int64_t a = 1;
    while (a < limit) {
        int64_t b = a + 1;
        while (b < limit) {
            int64_t a2b2 = a * a + b * b;
            int64_t c = 1;
            while (c * c < a2b2) {
                c += 1;
            }
            if (c * c == a2b2) {
                if (a + b + c <= limit) {
                    sum += a + b + c;
                }
            }
            b += 1;
        }
        a += 1;
    }
    return sum;
}

int main(void) {
    int64_t limit = 1500;
    int64_t total = Solve(limit);
    printf("Pythagorean Triples Sum: %lld\n", (long long)total);
    return 0;
}

