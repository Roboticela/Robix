#include <stdio.h>

static long long sum(const int *left, const int *right, int count) {
    long long total = 0;
    for (int index = 0; index < count; index++) {
        total += left[index] + right[index];
    }
    return total;
}

int main(void) {
    int left[1024];
    int right[1024];
    for (int index = 0; index < 1024; index++) {
        left[index] = index;
        right[index] = index;
    }
    printf("%lld\n", sum(left, right, 1024));
    return 0;
}
