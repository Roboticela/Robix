#include <cstdio>

struct Row {
    int hot;
    int also;
    int cold[16];
};

int main() {
    Row rows[1000];
    for (int index = 0; index < 1000; index++) {
        rows[index].hot = 1;
        rows[index].also = 2;
    }
    long long total = 0;
    for (int index = 0; index < 1000; index++) {
        total += rows[index].hot + rows[index].also;
    }
    std::printf("%lld\n", total);
    return 0;
}
