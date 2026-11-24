#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *values = malloc(1000 * sizeof(int));
    for (int index = 0; index < 1000; index++) {
        values[index] = index + 1;
    }
    long long total = 0;
    for (int index = 0; index < 1000; index++) {
        total += values[index];
    }
    printf("%lld\n", total);
    free(values);
    return 0;
}
