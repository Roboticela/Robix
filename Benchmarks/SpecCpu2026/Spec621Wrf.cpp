#include <iostream>
#include <iomanip>
#include <cstdint>

static double RunWrf(int64_t timeSteps) {
    double t0 = 288.15, t1 = 281.65, t2 = 275.15, t3 = 268.65, t4 = 262.15;
    double u0 = 5.0,    u1 = 12.0,   u2 = 20.0,   u3 = 30.0,   u4 = 45.0;
    double fCor = 0.0001;
    double dt = 0.5;

    int64_t step = 0;
    while (step < timeSteps) {
        double dt0 = -0.0065 * u0 * dt;
        double dt1 = -0.0065 * u1 * dt;
        double dt2 = -0.0065 * u2 * dt;
        double dt3 = -0.0065 * u3 * dt;
        double dt4 = -0.0065 * u4 * dt;

        t0 += dt0;
        t1 += dt1;
        t2 += dt2;
        t3 += dt3;
        t4 += dt4;

        u0 += (t0 - 280.0) * fCor * dt;
        u1 += (t1 - 275.0) * fCor * dt;
        u2 += (t2 - 270.0) * fCor * dt;
        u3 += (t3 - 265.0) * fCor * dt;
        u4 += (t4 - 260.0) * fCor * dt;

        step += 1;
    }

    return t0 + t1 + t2 + t3 + t4 + u0 + u1 + u2 + u3 + u4;
}

int main() {
    int64_t n = 10000000;
    double res = RunWrf(n);
    std::cout << "SPEC 621.wrf_r Checksum:\n" << std::fixed << std::setprecision(6) << res << "\n";
    return 0;
}

