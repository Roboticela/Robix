#include <stdio.h>

int main(void) {
    long long acc = 0;
    for (long long i = 0; i < 50000000; i = i + 1) {
        acc = acc + ((i ^ (i >> 1)) & 65535);
    }
    printf("%lld\n", acc);
    return 0;
}
