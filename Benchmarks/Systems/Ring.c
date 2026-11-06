#include <stdint.h>
#include <stdio.h>

int main(void) {
    int64_t buf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int64_t cap = 8;
    int64_t head = 0;
    int64_t tail = 0;
    int64_t count = 0;
    int64_t acc = 0;
    for (int64_t i = 0; i < 2000000; i = i + 1) {
        if ((i % 3) != 0) {
            if (count < cap) {
                buf[tail] = i % 97;
                tail = tail + 1;
                if (tail == cap) { tail = 0; }
                count = count + 1;
            }
        } else {
            if (count > 0) {
                acc = acc + buf[head];
                head = head + 1;
                if (head == cap) { head = 0; }
                count = count - 1;
            }
        }
    }
    while (count > 0) {
        acc = acc + buf[head];
        head = head + 1;
        if (head == cap) { head = 0; }
        count = count - 1;
    }
    printf("%lld\n", (long long)acc);
    return 0;
}
