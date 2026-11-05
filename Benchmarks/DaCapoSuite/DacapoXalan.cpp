#include <iostream>
#include <cstdint>

static int64_t RunXalanTransform(int64_t numNodes) {
    int64_t seed = 999888777;
    int64_t checksum = 0;

    int64_t depth = 0;
    int64_t tagCounter = 0;

    int64_t i = 0;
    while (i < numNodes) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t eventType = (seed / 65536) % 3;

        if (eventType == 0) {
            depth += 1;
            tagCounter += 1;
            checksum = (checksum * 33 + tagCounter + depth) % 1000000007;
        } else if (eventType == 1) {
            if (depth > 0) depth -= 1;
            checksum = (checksum * 31 + depth) % 1000000007;
        } else {
            int64_t textLen = (seed / 16384) % 64 + 1;
            checksum = (checksum * 37 + textLen * depth) % 1000000007;
        }

        i += 1;
    }

    return checksum;
}

int main() {
    int64_t n = 5000000;
    int64_t res = RunXalanTransform(n);
    std::cout << "DaCapo Xalan Checksum:\n" << res << "\n";
    return 0;
}

