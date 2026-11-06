#include <stdint.h>
#include <stdio.h>

int main() {
    int64_t x = 0;
    int64_t y = 0;
    int64_t vx = 3;
    int64_t vy = 0 - 1;
    for (int64_t i = 0; i < 2000000; i = i + 1) {
        vx = vx + (i % 5) - 2;
        vy = vy + (i % 3) - 1;
        if (vx > 20) { vx = 20; }
        if (vx < 0 - 20) { vx = 0 - 20; }
        if (vy > 20) { vy = 20; }
        if (vy < 0 - 20) { vy = 0 - 20; }
        x = x + vx;
        y = y + vy;
        if (x > 100000) { x = x - 100000; }
        if (x < 0) { x = x + 100000; }
        if (y > 100000) { y = y - 100000; }
        if (y < 0) { y = y + 100000; }
    }
    printf("%lld\n", (long long)(x + y + vx + vy));
    return 0;
}
