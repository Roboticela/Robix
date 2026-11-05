#include <iostream>
#include <iomanip>
#include <cstdint>

static double RunSunflowRays(int64_t numRays) {
    int64_t seed = 718281828;
    double totalIntensity = 0.0;
    int64_t hitCount = 0;

    int64_t i = 0;
    while (i < numRays) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        double dx = 0.002 * (double)((seed / 65536) % 1000) - 1.0;
        seed = (seed * 1103515245 + 12345) % 2147483648;
        double dy = 0.002 * (double)((seed / 65536) % 1000) - 1.0;
        double dz = 5.0;

        double dirNorm = dx * dx + dy * dy + dz * dz;
        double bCoeff = -20.0 * dz;
        double cCoeff = 96.0;

        double discriminant = bCoeff * bCoeff - 4.0 * dirNorm * cCoeff;
        if (discriminant >= 0.0) {
            double tHit = -bCoeff / (2.0 * dirNorm);
            double hitZ = tHit * dz;
            double normalZ = (hitZ - 10.0) / 2.0;
            if (normalZ < 0.0) normalZ = -normalZ;

            totalIntensity += normalZ;
            hitCount += 1;
        }

        i += 1;
    }

    return totalIntensity + (double)hitCount;
}

int main() {
    int64_t n = 5000000;
    double res = RunSunflowRays(n);
    std::cout << "DaCapo Sunflow Checksum:\n" << std::fixed << std::setprecision(6) << res << "\n";
    return 0;
}

