package Benchmarks.SpecCpu.Wrf;

public Function RunWrf(Int timeSteps) -> Float {
    // 5-level atmospheric column advection and Coriolis physics
    Float t0 = 288.15; Float t1 = 281.65; Float t2 = 275.15; Float t3 = 268.65; Float t4 = 262.15;
    Float u0 = 5.0;    Float u1 = 12.0;   Float u2 = 20.0;   Float u3 = 30.0;   Float u4 = 45.0;
    Float fCor = 0.0001; // Coriolis parameter
    Float dt = 0.5;

    Int step = 0;
    while (step < timeSteps) {
        // Vertical advection and thermodynamic lapse cooling
        Float dt0 = -0.0065 * u0 * dt;
        Float dt1 = -0.0065 * u1 * dt;
        Float dt2 = -0.0065 * u2 * dt;
        Float dt3 = -0.0065 * u3 * dt;
        Float dt4 = -0.0065 * u4 * dt;

        t0 = t0 + dt0;
        t1 = t1 + dt1;
        t2 = t2 + dt2;
        t3 = t3 + dt3;
        t4 = t4 + dt4;

        // Geostrophic wind shear adjustment
        u0 = u0 + (t0 - 280.0) * fCor * dt;
        u1 = u1 + (t1 - 275.0) * fCor * dt;
        u2 = u2 + (t2 - 270.0) * fCor * dt;
        u3 = u3 + (t3 - 265.0) * fCor * dt;
        u4 = u4 + (t4 - 260.0) * fCor * dt;

        step = step + 1;
    }

    return t0 + t1 + t2 + t3 + t4 + u0 + u1 + u2 + u3 + u4;
}

public Function Main() -> Void {
    Int n = 10000000;
    Float res = RunWrf(n);
    Print("SPEC 621.wrf_r Checksum:");
    Print(res);
}

