package Benchmarks.PIDLoop;

public Function SimulatePID(Int steps, Float target) -> Float {
    Float kp = 1.25;
    Float ki = 0.05;
    Float kd = 0.20;
    Float dt = 0.01;

    Float currentVal = 0.0;
    Float integral = 0.0;
    Float prevError = 0.0;

    Int i = 0;
    while (i < steps) {
        Float error = target - currentVal;
        integral = integral + error * dt;
        Float derivative = (error - prevError) / dt;
        Float output = kp * error + ki * integral + kd * derivative;
        currentVal = currentVal + output * dt;
        prevError = error;
        i = i + 1;
    }
    return currentVal;
}

public Function Main() -> Void {
    Int iterations = 20000000;
    Float finalVal = SimulatePID(iterations, 100.0);
    Print("Robix Native PID Simulation Output:");
    Print(finalVal);
}

