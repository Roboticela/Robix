package Benchmarks.NPB.FT;

public Function RunFT(Int numSteps) -> Float {
    // 4-point Complex Cooley-Tukey Radix-2 FFT kernel
    // Real and Imaginary parts: x0, x1, x2, x3
    Float r0 = 1.0; Float i0 = 0.0;
    Float r1 = 0.5; Float i1 = 0.5;
    Float r2 = 0.0; Float i2 = 1.0;
    Float r3 = 0.5 - 1.0; Float i3 = 0.5;

    Int step = 0;
    while (step < numSteps) {
        // Stage 1 butterflies: (0, 2) and (1, 3)
        Float u0R = r0 + r2; Float u0I = i0 + i2;
        Float u2R = r0 - r2; Float u2I = i0 - i2;

        Float u1R = r1 + r3; Float u1I = i1 + i3;
        Float diffR = r1 - r3; Float diffI = i1 - i3;
        // Twiddle W = -i => (diffR + i*diffI)*(-i) = diffI - i*diffR
        Float u3R = diffI;
        Float u3I = 0.0 - diffR;

        // Stage 2 butterflies: (0, 1) and (2, 3)
        r0 = u0R + u1R; i0 = u0I + u1I;
        r1 = u0R - u1R; i1 = u0I - u1I;
        r2 = u2R + u3R; i2 = u2I + u3I;
        r3 = u2R - u3R; i3 = u2I - u3I;

        // Normalization scale factor
        r0 = r0 * 0.25; i0 = i0 * 0.25;
        r1 = r1 * 0.25; i1 = i1 * 0.25;
        r2 = r2 * 0.25; i2 = i2 * 0.25;
        r3 = r3 * 0.25; i3 = i3 * 0.25;

        step = step + 1;
    }

    return r0 + i0 + r1 + i1 + r2 + i2 + r3 + i3;
}

public Function Main() -> Void {
    Int n = 10000000;
    Float res = RunFT(n);
    Print("NPB FT Checksum:");
    Print(res);
}

