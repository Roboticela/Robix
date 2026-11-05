#include <iostream>
#include <iomanip>
#include <cstdint>

struct Body {
    double x, y, vx, vy, m;
};

class OrbitSimulator {
public:
    static double Simulate(int64_t steps) {
        Body b1{ 0.0, 0.0, 0.0, 0.1, 1000.0 };
        Body b2{ 10.0, 0.0, 0.0, -0.1, 1000.0 };
        double dt = 0.001;

        for (int64_t step = 0; step < steps; ++step) {
            double dx = b2.x - b1.x;
            double dy = b2.y - b1.y;
            double distSq = dx * dx + dy * dy + 0.001;
            double force = (b1.m * b2.m) / (distSq * distSq);

            double fx = force * (dx / distSq);
            double fy = force * (dy / distSq);

            b1.vx += (fx / b1.m) * dt;
            b1.vy += (fy / b1.m) * dt;
            b1.x += b1.vx * dt;
            b1.y += b1.vy * dt;

            b2.vx -= (fx / b2.m) * dt;
            b2.vy -= (fy / b2.m) * dt;
            b2.x += b2.vx * dt;
            b2.y += b2.vy * dt;
        }
        return b1.x + b1.y + b2.x + b2.y;
    }
};

int main() {
    int64_t steps = 5000000;
    double finalSum = OrbitSimulator::Simulate(steps);
    std::cout << "NBody Orbit Sum: " << std::fixed << std::setprecision(6) << finalSum << "\n";
    return 0;
}

