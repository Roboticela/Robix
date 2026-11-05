package Benchmarks.NPB.MG;

public Function RunMG(Int numCycles) -> Float {
    // 2-level geometric multigrid V-cycle on 4-point 1D mesh
    // Fine grid: u0, u1, u2, u3 | Coarse grid: uc0, uc1
    Float u0 = 0.0; Float u1 = 0.0; Float u2 = 0.0; Float u3 = 0.0;
    Float r0 = 1.0; Float r1 = 2.0; Float r2 = 1.5; Float r3 = 0.5;

    Int cycle = 0;
    while (cycle < numCycles) {
        // 1. Pre-smoothing: Jacobi relaxation u = u + 0.5 * r
        u0 = u0 + 0.5 * r0;
        u1 = u1 + 0.5 * r1;
        u2 = u2 + 0.5 * r2;
        u3 = u3 + 0.5 * r3;

        // 2. Residual computation: r = f - A*u
        r0 = 1.0 - (2.0 * u0 - u1);
        r1 = 2.0 - (2.0 * u1 - u0 - u2);
        r2 = 1.5 - (2.0 * u2 - u1 - u3);
        r3 = 0.5 - (2.0 * u3 - u2);

        // 3. Restriction: full weighting to coarse grid
        Float rc0 = 0.5 * r0 + 0.25 * r1;
        Float rc1 = 0.25 * r2 + 0.5 * r3;

        // 4. Coarse grid direct solve (2x2 system)
        Float uc0 = 0.6 * rc0 + 0.2 * rc1;
        Float uc1 = 0.2 * rc0 + 0.6 * rc1;

        // 5. Prolongation: linear interpolation correction
        u0 = u0 + uc0;
        u1 = u1 + 0.75 * uc0 + 0.25 * uc1;
        u2 = u2 + 0.25 * uc0 + 0.75 * uc1;
        u3 = u3 + uc1;

        // 6. Post-smoothing
        u0 = u0 + 0.25 * r0;
        u1 = u1 + 0.25 * r1;
        u2 = u2 + 0.25 * r2;
        u3 = u3 + 0.25 * r3;

        cycle = cycle + 1;
    }

    return u0 + u1 + u2 + u3;
}

public Function Main() -> Void {
    Int n = 5000000;
    Float res = RunMG(n);
    Print("NPB MG Checksum:");
    Print(res);
}

