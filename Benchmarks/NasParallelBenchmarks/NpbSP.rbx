package Benchmarks.NPB.SP;

public Function RunSP(Int numSteps) -> Float {
    // 5-point Scalar Penta-diagonal solver
    // Pentadiagonal system matrix: [A B C D E]
    // u[i] = rhs - (a*u[i-2] + b*u[i-1] + d*u[i+1] + e*u[i+2]) / c
    Float u0 = 1.0; Float u1 = 1.0; Float u2 = 1.0; Float u3 = 1.0; Float u4 = 1.0;
    Float rhs0 = 2.0; Float rhs1 = 2.5; Float rhs2 = 3.0; Float rhs3 = 2.5; Float rhs4 = 2.0;

    Float cDiag = 4.0;
    Float off1 = -1.0;
    Float off2 = 0.2;

    Int step = 0;
    while (step < numSteps) {
        // Jacobi-like relaxation sweep for pentadiagonal system
        Float nu0 = (rhs0 - (off1 * u1 + off2 * u2)) / cDiag;
        Float nu1 = (rhs1 - (off1 * (u0 + u2) + off2 * u3)) / cDiag;
        Float nu2 = (rhs2 - (off1 * (u1 + u3) + off2 * (u0 + u4))) / cDiag;
        Float nu3 = (rhs3 - (off1 * (u2 + u4) + off2 * u1)) / cDiag;
        Float nu4 = (rhs4 - (off1 * u3 + off2 * u2)) / cDiag;

        u0 = nu0; u1 = nu1; u2 = nu2; u3 = nu3; u4 = nu4;

        // Feedback step
        rhs2 = rhs2 + 0.0001 * u2;

        step = step + 1;
    }

    return u0 + u1 + u2 + u3 + u4;
}

public Function Main() -> Void {
    Int n = 5000000;
    Float res = RunSP(n);
    Print("NPB SP Checksum:");
    Print(res);
}

