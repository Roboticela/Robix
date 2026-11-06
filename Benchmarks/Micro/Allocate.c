#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int64_t sum = 0;
    for (int64_t i = 0; i < 20000; i = i + 1) {
        uint8_t* block = (uint8_t*)malloc(64);
        if (block == NULL) {
            printf("0\n");
            return 1;
        }
        *block = 7;
        uint8_t stored = *block;
        sum = sum + (int64_t)stored;
        free(block);
    }
    printf("%lld\n", (long long)sum);
    return 0;
}
