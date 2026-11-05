#include <iostream>
#include <iomanip>
#include <cstdint>

class PIDController {
private:
    double kp;
    double ki;
    double kd;
    double dt;
    double currentVal;
    double integral;
    double prevError;

public:
    PIDController(double p, double i, double d, double delta)
        : kp(p), ki(i), kd(d), dt(delta), currentVal(0.0), integral(0.0), prevError(0.0) {}

    double Update(double target) {
        double error = target - currentVal;
        integral += error * dt;
        double derivative = (error - prevError) / dt;
        double output = kp * error + ki * integral + kd * derivative;
        currentVal += output * dt;
        prevError = error;
        return currentVal;
    }

    double GetValue() const { return currentVal; }
};

int main() {
    int64_t iterations = 20000000;
    PIDController pid(1.25, 0.05, 0.20, 0.01);
    for (int64_t i = 0; i < iterations; ++i) {
        pid.Update(100.0);
    }
    std::cout << "PID OOP Output: " << std::fixed << std::setprecision(2) << pid.GetValue() << "\n";
    return 0;
}

