package Benchmarks.NPB.UA;

public Function RunUA(Int numSteps) -> Float {
    // Unstructured Adaptive Mesh dynamic element refinement & face interpolation
    Float e0 = 1.0; Float e1 = 0.5; Float e2 = 0.25; Float e3 = 0.125;
    Float f01 = 0.75; Float f12 = 0.375; Float f23 = 0.1875;

    Int step = 0;
    while (step < numSteps) {
        // Face flux integration
        Float flux0 = f01 - (e0 - e1);
        Float flux1 = f12 - (e1 - e2);
        Float flux2 = f23 - (e2 - e3);

        e0 = e0 + 0.1 * flux0;
        e1 = e1 + 0.1 * (flux1 - flux0);
        e2 = e2 + 0.1 * (flux2 - flux1);
        e3 = e3 - 0.1 * flux2;

        // Dynamic refinement adaptation
        f01 = 0.5 * (e0 + e1);
        f12 = 0.5 * (e1 + e2);
        f23 = 0.5 * (e2 + e3);

        step = step + 1;
    }

    return e0 + e1 + e2 + e3 + f01 + f12 + f23;
}

public Function Main() -> Void {
    Int n = 10000000;
    Float res = RunUA(n);
    Print("NPB UA Checksum:");
    Print(res);
}

