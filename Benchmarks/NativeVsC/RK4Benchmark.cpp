#include <iostream>
#include <iomanip>
#include <cstdint>

class IntegratorRK4 {
public:
    static double Step(double x, double v, double dt) {
        double k1x = v;
        double k1v = -x;

        double k2x = v + 0.5 * dt * k1v;
        double k2v = -(x + 0.5 * dt * k1x);

        double k3x = v + 0.5 * dt * k2v;
        double k3v = -(x + 0.5 * dt * k2x);

        double k4x = v + dt * k3v;
        double k4v = -(x + dt * k3x);

        return x + (dt / 6.0) * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
    }
};

int main() {
    int64_t steps = 20000000;
    double x = 1.0;
    double v = 0.0;
    double dt = 0.001;

    for (int64_t i = 0; i < steps; ++i) {
        double nextX = IntegratorRK4::Step(x, v, dt);
        v -= x * dt;
        x = nextX;
    }
    std::cout << "RK4 Position: " << std::fixed << std::setprecision(6) << x << "\n";
    return 0;
}

