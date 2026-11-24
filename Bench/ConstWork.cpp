#include <cstdio>

static int work(int left, int right) {
    return left * 1000 + right;
}

int main() {
    std::printf("%d\n", work(20, 22));
    return 0;
}
