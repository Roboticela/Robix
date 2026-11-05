#include <iostream>
#include <cstdint>

static int64_t GenerateFasta(int64_t n) {
    int64_t seed = 42;
    int64_t countA = 0;
    int64_t countC = 0;
    int64_t countG = 0;
    int64_t countT = 0;
    int64_t hash = 0;

    int64_t i = 0;
    while (i < n) {
        seed = (seed * 3877 + 29573) % 139968;
        double p = (double)seed / 139968.0;
        int64_t ch = 84;
        if (p < 0.27) {
            ch = 65;
            countA += 1;
        } else if (p < 0.57) {
            ch = 67;
            countC += 1;
        } else if (p < 0.87) {
            ch = 71;
            countG += 1;
        } else {
            countT += 1;
        }
        hash = (hash * 31 + ch) % 1000000007;
        i += 1;
    }
    return hash + countA + countC + countG + countT;
}

int main() {
    int64_t n = 5000000;
    int64_t res = GenerateFasta(n);
    std::cout << "CPP Fasta Result:\n" << res << "\n";
    return 0;
}

