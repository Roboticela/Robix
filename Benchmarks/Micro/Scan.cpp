#include <stdint.h>
#include <stdio.h>

int main() {
    int64_t count = 0;
    for (int64_t i = 0; i < 8000000; i = i + 1) {
        int64_t code = (i * 17 + 3) % 251;
        if (code == 7) {
            count = count + 1;
        }
    }
    printf("%lld\n", (long long)count);
    return 0;
}
