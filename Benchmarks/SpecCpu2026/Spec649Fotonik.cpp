#include <iostream>
#include <iomanip>
#include <cstdint>

static double RunFotonik3D(int64_t timeSteps) {
    double ez0 = 0.0, ez1 = 0.0, ez2 = 0.0;
    double hy0 = 0.0, hy1 = 0.0, hy2 = 0.0;
    double c0 = 0.5;

    int64_t t = 0;
    while (t < timeSteps) {
        double src = (t % 2 == 1) ? -1.0 : 1.0;
        ez1 += src;

        ez0 += c0 * hy0;
        ez1 += c0 * (hy1 - hy0);
        ez2 += c0 * (hy2 - hy1);

        hy0 += c0 * (ez1 - ez0);
        hy1 += c0 * (ez2 - ez1);
        hy2 -= c0 * ez2;

        t += 1;
    }

    return ez0 + ez1 + ez2 + hy0 + hy1 + hy2;
}

int main() {
    int64_t n = 10000000;
    double res = RunFotonik3D(n);
    std::cout << "SPEC 649.fotonik3d_r Checksum:\n" << std::fixed << std::setprecision(6) << res << "\n";
    return 0;
}

