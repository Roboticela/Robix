#include <iostream>
#include <cstdint>

static int64_t RunBiojavaNeedlemanWunsch(int64_t seqLen) {
    int64_t dp0 = 0, dp1 = -2, dp2 = -4, dp3 = -6;

    int64_t seedA = 1234567;
    int64_t seedB = 7654321;

    int64_t i = 0;
    while (i < seqLen) {
        // Step 1
        seedA = (seedA * 1103515245 + 12345) % 2147483648;
        seedB = (seedB * 1103515245 + 12345) % 2147483648;
        int64_t charA0 = (seedA / 65536) % 4;
        int64_t charB0 = (seedB / 65536) % 4;

        int64_t matchScore0 = -1;
        if (charA0 == charB0) matchScore0 = 2;

        int64_t diag0 = dp0 + matchScore0;
        int64_t up0 = dp1 - 2;
        int64_t left0 = dp2 - 2;

        int64_t nextDp0 = diag0;
        if (up0 > nextDp0) nextDp0 = up0;
        if (left0 > nextDp0) nextDp0 = left0;

        dp0 = dp1;
        dp1 = dp2;
        dp2 = dp3;
        dp3 = nextDp0;

        // Step 2
        seedA = (seedA * 1103515245 + 12345) % 2147483648;
        seedB = (seedB * 1103515245 + 12345) % 2147483648;
        int64_t charA1 = (seedA / 65536) % 4;
        int64_t charB1 = (seedB / 65536) % 4;

        int64_t matchScore1 = -1;
        if (charA1 == charB1) matchScore1 = 2;

        int64_t diag1 = dp0 + matchScore1;
        int64_t up1 = dp1 - 2;
        int64_t left1 = dp2 - 2;

        int64_t nextDp1 = diag1;
        if (up1 > nextDp1) nextDp1 = up1;
        if (left1 > nextDp1) nextDp1 = left1;

        dp0 = dp1;
        dp1 = dp2;
        dp2 = dp3;
        dp3 = nextDp1;

        i += 2;
    }

    return dp0 + dp1 + dp2 + dp3;
}

int main() {
    int64_t n = 10000000;
    int64_t res = RunBiojavaNeedlemanWunsch(n);
    std::cout << "DaCapo Biojava Checksum:\n" << res << "\n";
    return 0;
}
