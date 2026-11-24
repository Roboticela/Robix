#include <stdio.h>

int main(void) {
    long long total = 0;
    for (long long index = 1; index <= 1000000; index++) {
        total += index;
    }
    printf("%lld\n", total);
    return 0;
}
