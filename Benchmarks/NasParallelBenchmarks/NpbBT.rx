package Benchmarks.NPB.BT;

public Function RunBT(Int numSteps) -> Float {
    // 3-variable Block Tri-diagonal system solver
    // [ B0  C0   0 ] [ u0 ]   [ r0 ]
    // [ A1  B1  C1 ] [ u1 ] = [ r1 ]
    // [  0  A2  B2 ] [ u2 ]   [ r2 ]
    Float u0 = 1.0; Float u1 = 1.0; Float u2 = 1.0;
    Float r0 = 2.0; Float r1 = 3.0; Float r2 = 1.5;

    Float a1 = -0.5; Float a2 = -0.5;
    Float b0 = 2.0;  Float b1 = 2.0;  Float b2 = 2.0;
    Float c0 = -0.5; Float c1 = -0.5;

    Int step = 0;
    while (step < numSteps) {
        // Forward elimination (Thomas algorithm block variant)
        Float invB0 = 1.0 / b0;
        Float cPrime0 = c0 * invB0;
        Float dPrime0 = r0 * invB0;

        Float denom1 = b1 - a1 * cPrime0;
        Float invDenom1 = 1.0 / denom1;
        Float cPrime1 = c1 * invDenom1;
        Float dPrime1 = (r1 - a1 * dPrime0) * invDenom1;

        Float denom2 = b2 - a2 * cPrime1;
        Float dPrime2 = (r2 - a2 * dPrime1) / denom2;

        // Back substitution
        u2 = dPrime2;
        u1 = dPrime1 - cPrime1 * u2;
        u0 = dPrime0 - cPrime0 * u1;

        // Time advance step coupling
        r0 = r0 + 0.001 * u0;
        r1 = r1 + 0.001 * u1;
        r2 = r2 + 0.001 * u2;

        step = step + 1;
    }

    return u0 + u1 + u2 + r0 + r1 + r2;
}

public Function Main() -> Void {
    Int n = 5000000;
    Float res = RunBT(n);
    Print("NPB BT Checksum:");
    Print(res);
}

