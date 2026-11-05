#include <iostream>
#include <iomanip>
#include <cstdint>

static double RunBatikSVG(int64_t numPaths) {
    double p0X = 0.0, p0Y = 0.0;
    double p1X = 10.0, p1Y = 30.0;
    double p2X = 50.0, p2Y = 60.0;
    double p3X = 100.0, p3Y = 10.0;

    double sumX = 0.0, sumY = 0.0;

    int64_t i = 0;
    while (i < numPaths) {
        double t = 0.5;
        double oneMinusT = 1.0 - t;

        double c0 = oneMinusT * oneMinusT * oneMinusT;
        double c1 = 3.0 * oneMinusT * oneMinusT * t;
        double c2 = 3.0 * oneMinusT * t * t;
        double c3 = t * t * t;

        double bx = c0 * p0X + c1 * p1X + c2 * p2X + c3 * p3X;
        double by = c0 * p0Y + c1 * p1Y + c2 * p2Y + c3 * p3Y;

        sumX += bx;
        sumY += by;

        p1X += 0.0001;
        p2Y += 0.0001;

        i += 1;
    }

    return sumX + sumY;
}

int main() {
    int64_t n = 10000000;
    double res = RunBatikSVG(n);
    std::cout << "DaCapo Batik Checksum:\n" << std::fixed << std::setprecision(6) << res << "\n";
    return 0;
}

