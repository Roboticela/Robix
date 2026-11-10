package Benchmarks.NBody;

public Function SimulateNBody(Int steps) -> Float {
    Float x1 = 0.0;
    Float y1 = 0.0;
    Float vx1 = 0.0;
    Float vy1 = 0.1;
    Float m1 = 1000.0;

    Float x2 = 10.0;
    Float y2 = 0.0;
    Float vx2 = 0.0;
    Float vy2 = 0.0 - 0.1;
    Float m2 = 1000.0;

    Float dt = 0.001;
    Int step = 0;
    while (step < steps) {
        Float dx = x2 - x1;
        Float dy = y2 - y1;
        Float distSq = dx * dx + dy * dy + 0.001;
        Float force = 1000000.0 / (distSq * distSq);

        Float fx = force * dx / distSq;
        Float fy = force * dy / distSq;

        vx1 = vx1 + (fx / 1000.0) * dt;
        vy1 = vy1 + (fy / 1000.0) * dt;
        x1 = x1 + vx1 * dt;
        y1 = y1 + vy1 * dt;

        vx2 = vx2 - (fx / 1000.0) * dt;
        vy2 = vy2 - (fy / 1000.0) * dt;
        x2 = x2 + vx2 * dt;
        y2 = y2 + vy2 * dt;

        step = step + 1;
    }
    return x1 + y1 + x2 + y2;
}

public Function Main() -> Void {
    Int steps = 5000000;
    Float finalSum = SimulateNBody(steps);
    Print("Robix Native N-Body Orbit Sum:");
    Print(finalSum);
}
