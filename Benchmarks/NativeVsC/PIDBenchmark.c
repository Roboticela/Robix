#include <stdio.h>
#include <stdint.h>

static double SimulatePID(int64_t steps, double target) {
    double kp = 1.25;
    double ki = 0.05;
    double kd = 0.20;
    double dt = 0.01;

    double currentVal = 0.0;
    double integral = 0.0;
    double prevError = 0.0;

    int64_t i = 0;
    while (i < steps) {
        double error = target - currentVal;
        integral = integral + error * dt;
        double derivative = (error - prevError) / dt;
        double output = kp * error + ki * integral + kd * derivative;
        currentVal = currentVal + output * dt;
        prevError = error;
        i = i + 1;
    }
    return currentVal;
}

int main(void) {
    int64_t iterations = 20000000;
    double finalVal = SimulatePID(iterations, 100.0);
    printf("PID Output: %.2f\n", finalVal);
    return 0;
}

