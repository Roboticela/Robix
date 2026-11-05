#include <iostream>
#include <cstdint>

static int64_t CheckTree(int64_t depth, int64_t item) {
    if (depth <= 0) return item;
    int64_t left = CheckTree(depth - 1, 2 * item - 1);
    int64_t right = CheckTree(depth - 1, 2 * item);
    return item + left - right;
}

static int64_t RunBinaryTrees(int64_t maxDepth) {
    int64_t checkSum = 0;
    int64_t stretchDepth = maxDepth + 1;
    checkSum += CheckTree(stretchDepth, 0);

    int64_t longLived = CheckTree(maxDepth, 0);

    int64_t d = 4;
    while (d <= maxDepth) {
        int64_t iterations = 1;
        int64_t k = 0;
        int64_t power = maxDepth - d + 4;
        while (k < power) {
            iterations *= 2;
            k += 1;
        }

        int64_t check = 0;
        int64_t i = 1;
        while (i <= iterations) {
            check += CheckTree(d, i);
            check += CheckTree(d, -i);
            i += 1;
        }
        checkSum += check;
        d += 2;
    }

    checkSum += longLived;
    return checkSum;
}

int main() {
    int64_t maxDepth = 16;
    int64_t res = RunBinaryTrees(maxDepth);
    std::cout << "CPP Binary-Trees Result:\n" << res << "\n";
    return 0;
}

