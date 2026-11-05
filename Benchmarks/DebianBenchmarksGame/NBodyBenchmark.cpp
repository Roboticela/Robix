#include <iostream>
#include <iomanip>
#include <cstdint>

static double SimulateNBody(int64_t steps) {
    double x1 = 0.0, y1 = 0.0, vx1 = 0.0, vy1 = 0.1, m1 = 1000.0;
    double x2 = 10.0, y2 = 0.0, vx2 = 0.0, vy2 = -0.1, m2 = 1000.0;
    double dt = 0.001;
    int64_t step = 0;
    while (step < steps) {
        double dx = x2 - x1;
        double dy = y2 - y1;
        double distSq = dx * dx + dy * dy + 0.001;
        double force = (m1 * m2) / (distSq * distSq);

        double fx = force * (dx / distSq);
        double fy = force * (dy / distSq);

        vx1 += (fx / m1) * dt;
        vy1 += (fy / m1) * dt;
        x1 += vx1 * dt;
        y1 += vy1 * dt;

        vx2 -= (fx / m2) * dt;
        vy2 -= (fy / m2) * dt;
        x2 += vx2 * dt;
        y2 += vy2 * dt;

        step += 1;
    }
    return x1 + y1 + x2 + y2;
}

int main() {
    int64_t steps = 5000000;
    double finalSum = SimulateNBody(steps);
    std::cout << std::fixed << std::setprecision(6) << "CPP N-Body Orbit Sum:\n" << finalSum << "\n";
    return 0;
}

