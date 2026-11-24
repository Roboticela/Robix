#include <cstdio>

int main() {
    long long total = 0;
    for (long long index = 1; index <= 1000000; index++) {
        total += index;
    }
    std::printf("%lld\n", total);
    return 0;
}
