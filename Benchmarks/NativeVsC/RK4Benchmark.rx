package Benchmarks.RK4;

public Function StepRK4(Float x, Float v, Float dt) -> Float {
    Float k1x = v;
    Float k1v = 0.0 - x;

    Float k2x = v + 0.5 * dt * k1v;
    Float k2v = 0.0 - (x + 0.5 * dt * k1x);

    Float k3x = v + 0.5 * dt * k2v;
    Float k3v = 0.0 - (x + 0.5 * dt * k2x);

    Float k4x = v + dt * k3v;
    Float k4v = 0.0 - (x + dt * k3x);

    Float nextX = x + (dt / 6.0) * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
    return nextX;
}

public Function Main() -> Void {
    Int steps = 20000000;
    Float x = 1.0;
    Float v = 0.0;
    Float dt = 0.001;

    Int i = 0;
    while (i < steps) {
        Float nextX = StepRK4(x, v, dt);
        v = v - x * dt;
        x = nextX;
        i = i + 1;
    }

    Print("Robix Native RK4 Harmonic Oscillator Position:");
    Print(x);
}

