package Benchmarks.NPB.LU;

public Function RunLU(Int numIters) -> Float {
    // Lower-Upper Gauss-Seidel solver sweep on 4-variable state
    Float x0 = 0.0; Float x1 = 0.0; Float x2 = 0.0; Float x3 = 0.0;
    Float b0 = 1.0; Float b1 = 2.0; Float b2 = 3.0; Float b3 = 2.0;

    Int iter = 0;
    while (iter < numIters) {
        // Forward sweep (Lower triangular inversion)
        x0 = b0 / 3.0;
        x1 = (b1 - (-1.0 * x0)) / 3.0;
        x2 = (b2 - (-1.0 * x1)) / 3.0;
        x3 = (b3 - (-1.0 * x2)) / 3.0;

        // Backward sweep (Upper triangular inversion)
        x3 = (b3 - (-0.5 * x2)) / 3.0;
        x2 = (b2 - (-0.5 * x3 + -1.0 * x1)) / 3.0;
        x1 = (b1 - (-0.5 * x2 + -1.0 * x0)) / 3.0;
        x0 = (b0 - (-0.5 * x1)) / 3.0;

        // Dynamic RHS forcing update
        b1 = b1 + 0.0001 * x1;
        b2 = b2 + 0.0001 * x2;

        iter = iter + 1;
    }

    return x0 + x1 + x2 + x3 + b0 + b1 + b2 + b3;
}

public Function Main() -> Void {
    Int n = 5000000;
    Float res = RunLU(n);
    Print("NPB LU Checksum:");
    Print(res);
}

