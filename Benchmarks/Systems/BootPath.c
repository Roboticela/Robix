#include <stdint.h>
#include <stdio.h>

int main(void) {
    int64_t addr = 0;
    for (int64_t i = 0; i < 3000000; i = i + 1) {
        int64_t page = i / 512;
        int64_t slot = i % 512;
        addr = addr + page * 8 + slot;
    }
    printf("%lld\n", (long long)addr);
    return 0;
}
