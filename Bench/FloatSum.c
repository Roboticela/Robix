#include <stdio.h>

int main(void) {
    double total = 0.0;
    for (long long index = 1; index <= 1000000; index++) {
        total += 1.5;
    }
    printf("%f\n", total);
    return total > 0.0 ? 1 : 0;
}
