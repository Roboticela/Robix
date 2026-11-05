#include <iostream>
#include <cstdint>

static int64_t RunMcfSim(int64_t n) {
    int64_t flow = 1000;
    int64_t cost = 0;
    int64_t priceA = 10, priceB = 25, priceC = 40, priceD = 15;

    int64_t iter = 0;
    while (iter < n) {
        int64_t reducedCost1 = priceB - priceA + 12;
        int64_t reducedCost2 = priceC - priceB + 8;
        int64_t reducedCost3 = priceD - priceC + 15;
        int64_t reducedCost4 = priceA - priceD + 5;

        if (reducedCost1 < 0) {
            priceA -= 2; priceB += 1; flow += 3;
            cost = (cost + flow * 12) % 1000000007;
        } else {
            priceA = (priceA + 1) % 50;
            cost = (cost + flow * 2) % 1000000007;
        }

        if (reducedCost2 < 0) {
            priceB -= 1; priceC += 2;
            cost = (cost + flow * 8) % 1000000007;
        } else {
            priceC = (priceC + 2) % 60;
            cost = (cost + 17) % 1000000007;
        }

        if (reducedCost3 < 0) {
            priceC -= 2; priceD += 3; flow += 1;
            cost = (cost + flow * 15) % 1000000007;
        } else {
            priceD = (priceD + 1) % 45;
            cost = (cost + 31) % 1000000007;
        }

        if (reducedCost4 < 0) {
            priceD -= 1; priceA += 2;
            cost = (cost + flow * 5) % 1000000007;
        } else {
            priceB = (priceB + 3) % 55;
            cost = (cost + 11) % 1000000007;
        }

        iter += 1;
    }

    return (cost + priceA + priceB + priceC + priceD + flow) % 1000000007;
}

int main() {
    int64_t n = 10000000;
    int64_t res = RunMcfSim(n);
    std::cout << "SPEC 605.mcf_r Checksum:\n" << res << "\n";
    return 0;
}

