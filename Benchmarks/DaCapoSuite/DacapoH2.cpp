#include <iostream>
#include <cstdint>

static int64_t RunH2Engine(int64_t numOps) {
    int64_t seed = 42424242;
    int64_t checksum = 0;

    int64_t t0Rows = 0, t0Sum = 0;
    int64_t t1Rows = 0, t1Sum = 0;
    int64_t t2Rows = 0, t2Sum = 0;
    int64_t t3Rows = 0, t3Sum = 0;

    int64_t i = 0;
    while (i < numOps) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        int64_t opType = (seed / 65536) % 3;
        int64_t table = (seed / 16384) % 4;
        int64_t val = (seed / 256) % 1000;

        if (opType == 0) {
            if (table == 0) {
                t0Rows += 1;
                t0Sum += val;
                if (t0Sum >= 1000000007) t0Sum -= 1000000007;
            } else if (table == 1) {
                t1Rows += 1;
                t1Sum += val;
                if (t1Sum >= 1000000007) t1Sum -= 1000000007;
            } else if (table == 2) {
                t2Rows += 1;
                t2Sum += val;
                if (t2Sum >= 1000000007) t2Sum -= 1000000007;
            } else {
                t3Rows += 1;
                t3Sum += val;
                if (t3Sum >= 1000000007) t3Sum -= 1000000007;
            }
        } else if (opType == 1) {
            if (table == 0) {
                t0Sum += val;
                if (t0Sum >= 1000000007) t0Sum -= 1000000007;
            } else if (table == 1) {
                t1Sum += val;
                if (t1Sum >= 1000000007) t1Sum -= 1000000007;
            } else if (table == 2) {
                t2Sum += val;
                if (t2Sum >= 1000000007) t2Sum -= 1000000007;
            } else {
                t3Sum += val;
                if (t3Sum >= 1000000007) t3Sum -= 1000000007;
            }
        } else {
            int64_t querySum = 0;
            if (table == 0) querySum = t0Sum + t0Rows;
            else if (table == 1) querySum = t1Sum + t1Rows;
            else if (table == 2) querySum = t2Sum + t2Rows;
            else querySum = t3Sum + t3Rows;
            checksum = (checksum * 31 + querySum) % 1000000007;
        }

        i += 1;
    }

    checksum = (checksum + t0Sum + t1Sum + t2Sum + t3Sum) % 1000000007;
    return checksum;
}

int main() {
    int64_t n = 5000000;
    int64_t res = RunH2Engine(n);
    std::cout << "DaCapo H2 Checksum:\n" << res << "\n";
    return 0;
}
