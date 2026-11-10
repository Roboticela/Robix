package Benchmarks.SpecCpu.Lbm;

public Function RunLbmSim(Int steps) -> Float {
    Float f0 = 0.25; Float f1 = 0.12; Float f2 = 0.08; Float f3 = 0.15;

    Int step = 0;
    while (step < steps) {
        // Step 1
        Float rho1 = f0 + f1 + f2 + f3;
        Float invRho1 = 1.0 / rho1;
        Float ux1 = (f1 - f3) * invRho1;
        Float uy1 = (f2 - f0) * invRho1;
        Float uSq1 = ux1 * ux1 + uy1 * uy1;

        Float feq0A = 0.25 * rho1 * (1.0 - 1.5 * uSq1);
        Float feq1A = 0.125 * rho1 * (1.0 + 3.0 * ux1 + 4.5 * ux1 * ux1 - 1.5 * uSq1);
        Float feq2A = 0.125 * rho1 * (1.0 + 3.0 * uy1 + 4.5 * uy1 * uy1 - 1.5 * uSq1);
        Float feq3A = 0.125 * rho1 * (1.0 - 3.0 * ux1 + 4.5 * ux1 * ux1 - 1.5 * uSq1);

        f0 = f0 - 1.45 * (f0 - feq0A);
        f1 = f1 - 1.45 * (f1 - feq1A);
        f2 = f2 - 1.45 * (f2 - feq2A);
        f3 = f3 - 1.45 * (f3 - feq3A);

        Float tmp1 = f1;
        f1 = f3;
        f3 = tmp1;

        // Step 2
        Float rho2 = f0 + f1 + f2 + f3;
        Float invRho2 = 1.0 / rho2;
        Float ux2 = (f1 - f3) * invRho2;
        Float uy2 = (f2 - f0) * invRho2;
        Float uSq2 = ux2 * ux2 + uy2 * uy2;

        Float feq0B = 0.25 * rho2 * (1.0 - 1.5 * uSq2);
        Float feq1B = 0.125 * rho2 * (1.0 + 3.0 * ux2 + 4.5 * ux2 * ux2 - 1.5 * uSq2);
        Float feq2B = 0.125 * rho2 * (1.0 + 3.0 * uy2 + 4.5 * uy2 * uy2 - 1.5 * uSq2);
        Float feq3B = 0.125 * rho2 * (1.0 - 3.0 * ux2 + 4.5 * ux2 * ux2 - 1.5 * uSq2);

        f0 = f0 - 1.45 * (f0 - feq0B);
        f1 = f1 - 1.45 * (f1 - feq1B);
        f2 = f2 - 1.45 * (f2 - feq2B);
        f3 = f3 - 1.45 * (f3 - feq3B);

        Float tmp2 = f1;
        f1 = f3;
        f3 = tmp2;

        step = step + 2;
    }

    return f0 + f1 + f2 + f3;
}

public Function Main() -> Void {
    Int n = 5000000;
    Float res = RunLbmSim(n);
    Print("SPEC 619.lbm_r Checksum:");
    Print(res);
}
