#include <iostream>
#include <cstdint>

static int64_t RunRevcomp(int64_t n) {
    int64_t seed = 987654321;
    int64_t hash = 0;
    int64_t i = 0;
    while (i < n) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t b1 = (seed / 16) % 4;
        int64_t b2 = (seed / 64) % 4;
        int64_t b3 = (seed / 256) % 4;
        int64_t b4 = (seed / 1024) % 4;

        int64_t c1 = 3 - b1;
        int64_t c2 = 3 - b2;
        int64_t c3 = 3 - b3;
        int64_t c4 = 3 - b4;

        hash = (hash * 33 + c4) % 1000000007;
        hash = (hash * 33 + c3) % 1000000007;
        hash = (hash * 33 + c2) % 1000000007;
        hash = (hash * 33 + c1) % 1000000007;

        i += 4;
    }
    return hash;
}

int main() {
    int64_t n = 10000000;
    int64_t res = RunRevcomp(n);
    std::cout << "CPP Reverse-Complement Result:\n" << res << "\n";
    return 0;
}

