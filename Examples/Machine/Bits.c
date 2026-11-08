#include <stdio.h>

static long long PageEntry(long long phys, long long flags) {
    return (phys & ~0xFFF) | flags;
}

int main(void) {
    long long entry = PageEntry(0x2000, 0x3);
    long long page = 1LL << 12;
    long long low = 0xFF & ~0x0F;
    long long mixed = (0xF0 ^ 0x0F) >> 1;
    long long negative = -8 >> 2;
    printf("%lld\n%lld\n%lld\n%lld\n%lld\n", entry, page, low, mixed, negative);
    return 0;
}
