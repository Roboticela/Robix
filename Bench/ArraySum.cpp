#include <cstdio>
#include <vector>

static long long sum(const int *left, const int *right, int count) {
    long long total = 0;
    for (int index = 0; index < count; index++) {
        total += left[index] + right[index];
    }
    return total;
}

int main() {
    std::vector<int> left(1024);
    std::vector<int> right(1024);
    for (int index = 0; index < 1024; index++) {
        left[index] = index;
        right[index] = index;
    }
    std::printf("%lld\n", sum(left.data(), right.data(), 1024));
    return 0;
}
