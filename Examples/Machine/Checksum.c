#include <stdio.h>

static long long PageEntry(long long phys, long long flags) {
    return (phys & ~0xFFF) | flags;
}

static long long SwitchChecksum(long long savedSp, long long savedIp) {
    long long sp = savedSp;
    long long ip = savedIp;
    long long nextSp = ip;
    long long nextIp = sp;
    return nextSp + nextIp;
}

static long long OnTimer(long long ticks) {
    __asm__ volatile("nop");
    return ticks + 1;
}

int main(void) {
    long long pages = PageEntry(8192, 3);
    long long switched = SwitchChecksum(100, 200);
    long long ticks = OnTimer(7);
    printf("%lld\n", pages + switched + ticks);
    return 0;
}
