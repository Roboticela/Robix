#include <iostream>
#include <cstdint>

static int64_t RunRegexRedux(int64_t n) {
    int64_t seed = 246813579;
    int64_t m1 = 0, m2 = 0, m3 = 0, m4 = 0;
    int64_t m5 = 0, m6 = 0, m7 = 0, m8 = 0;

    int64_t window = 0;
    int64_t i = 0;
    while (i < n) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t b = (seed / 65536) % 4;

        window = ((window * 4) + b) % 65536;

        if (window == 1335) m1 += 1;
        else if (window == 4821) m2 += 1;
        else if (window == 9204) m3 += 1;
        else if (window == 15420) m4 += 1;
        else if (window == 22100) m5 += 1;
        else if (window == 34091) m6 += 1;
        else if (window == 48112) m7 += 1;
        else if (window == 61003) m8 += 1;

        i += 1;
    }

    int64_t result = (m1 * 17) + (m2 * 23) + (m3 * 31) + (m4 * 41) +
                     (m5 * 47) + (m6 * 53) + (m7 * 59) + (m8 * 61);
    return result;
}

int main() {
    int64_t n = 10000000;
    int64_t res = RunRegexRedux(n);
    std::cout << "CPP Regex-Redux Result:\n" << res << "\n";
    return 0;
}

