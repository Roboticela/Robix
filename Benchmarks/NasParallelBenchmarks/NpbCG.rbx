package Benchmarks.NPB.CG;

public Function RunCG(Int numIters) -> Float {
    Float x0 = 1.0; Float x1 = 1.0; Float x2 = 1.0; Float x3 = 1.0;
    Float r0 = 0.5; Float r1 = 0.5; Float r2 = 0.5; Float r3 = 0.5;
    Float p0 = 0.5; Float p1 = 0.5; Float p2 = 0.5; Float p3 = 0.5;

    Int iter = 0;
    while (iter < numIters) {
        Float q0 = 3.0 * p0 - p1;
        Float q1 = 3.0 * p1 - p0 - p2;
        Float q2 = 3.0 * p2 - p1 - p3;
        Float q3 = 3.0 * p3 - p2;

        Float ddot = p0 * q0 + p1 * q1 + p2 * q2 + p3 * q3;
        Float rho = r0 * r0 + r1 * r1 + r2 * r2 + r3 * r3;
        Float alpha = rho / (ddot + 0.0001);

        x0 = x0 + alpha * p0;
        x1 = x1 + alpha * p1;
        x2 = x2 + alpha * p2;
        x3 = x3 + alpha * p3;

        r0 = r0 - alpha * q0;
        r1 = r1 - alpha * q1;
        r2 = r2 - alpha * q2;
        r3 = r3 - alpha * q3;

        Float rhoNext = r0 * r0 + r1 * r1 + r2 * r2 + r3 * r3;
        Float beta = rhoNext / (rho + 0.0001);

        p0 = r0 + beta * p0;
        p1 = r1 + beta * p1;
        p2 = r2 + beta * p2;
        p3 = r3 + beta * p3;

        iter = iter + 1;
    }

    return x0 + x1 + x2 + x3 + r0 + r1 + r2 + r3;
}

public Function Main() -> Void {
    Int n = 5000000;
    Float res = RunCG(n);
    Print("NPB CG Checksum:");
    Print(res);
}
