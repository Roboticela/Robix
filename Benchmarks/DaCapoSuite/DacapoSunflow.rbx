package Benchmarks.DaCapo.Sunflow;

public Function RunSunflowRays(Int numRays) -> Float {
    // Whitted-style Ray Tracing ray-sphere intersection
    // Sphere center: (0, 0, 10), radius: 2
    // Ray origin: (0, 0, 0), direction: (dx, dy, dz)
    Int seed = 718281828;
    Float totalIntensity = 0.0;
    Int hitCount = 0;

    Int i = 0;
    while (i < numRays) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Float dx = 0.002 * ((seed / 65536) % 1000).ToFloat() - 1.0;
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Float dy = 0.002 * ((seed / 65536) % 1000).ToFloat() - 1.0;
        Float dz = 5.0; // Pointing forward

        // Direction magnitude
        Float dirNorm = dx * dx + dy * dy + dz * dz;

        // Quadratic coefficients: oc = (0, 0, -10)
        // b = 2 * (dx*0 + dy*0 - 10*dz) = -20*dz
        // c = 100 - 4 = 96
        Float bCoeff = -20.0 * dz;
        Float cCoeff = 96.0;

        Float discriminant = bCoeff * bCoeff - 4.0 * dirNorm * cCoeff;
        if (discriminant >= 0.0) {
            // Hit detected: compute diffuse Lambertian shading
            Float tHit = (0.0 - bCoeff) / (2.0 * dirNorm);
            Float hitZ = tHit * dz;
            Float normalZ = (hitZ - 10.0) / 2.0;
            if (normalZ < 0.0) { normalZ = 0.0 - normalZ; }

            totalIntensity = totalIntensity + normalZ;
            hitCount = hitCount + 1;
        }

        i = i + 1;
    }

    return totalIntensity + hitCount.ToFloat();
}

public Function Main() -> Void {
    Int n = 5000000;
    Float res = RunSunflowRays(n);
    Print("DaCapo Sunflow Checksum:");
    Print(res);
}

