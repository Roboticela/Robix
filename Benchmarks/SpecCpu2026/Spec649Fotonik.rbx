package Benchmarks.SpecCpu.Fotonik;

public Function RunFotonik3D(Int timeSteps) -> Float {
    Float ez0 = 0.0; Float ez1 = 0.0; Float ez2 = 0.0;
    Float hy0 = 0.0; Float hy1 = 0.0; Float hy2 = 0.0;

    Int t = 0;
    while (t < timeSteps) {
        // Step 1: src = 1.0
        Float nez1 = ez1 + 1.0;
        ez0 = ez0 + 0.5 * hy0;
        Float m1 = nez1 + 0.5 * (hy1 - hy0);
        ez2 = ez2 + 0.5 * (hy2 - hy1);
        hy0 = hy0 + 0.5 * (m1 - ez0);
        hy1 = hy1 + 0.5 * (ez2 - m1);
        hy2 = hy2 - 0.5 * ez2;

        // Step 2: src = -1.0
        Float nez2 = m1 - 1.0;
        ez0 = ez0 + 0.5 * hy0;
        ez1 = nez2 + 0.5 * (hy1 - hy0);
        ez2 = ez2 + 0.5 * (hy2 - hy1);
        hy0 = hy0 + 0.5 * (ez1 - ez0);
        hy1 = hy1 + 0.5 * (ez2 - ez1);
        hy2 = hy2 - 0.5 * ez2;

        t = t + 2;
    }

    return ez0 + ez1 + ez2 + hy0 + hy1 + hy2;
}

public Function Main() -> Void {
    Int n = 10000000;
    Float res = RunFotonik3D(n);
    Print("SPEC 649.fotonik3d_r Checksum:");
    Print(res);
}
