package Benchmarks.NPB.EP;

public Function RunEP(Int numPairs) -> Float {
    Int seed = 271828182;
    Float sumX = 0.0;
    Float sumY = 0.0;
    Int insideCount = 0;

    Int i = 0;
    while (i < numPairs) {
        // Step 1
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int randX1 = (seed / 65536) % 1000;
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int randY1 = (seed / 65536) % 1000;

        Float x1 = 0.002 * randX1.ToFloat() - 1.0;
        Float y1 = 0.002 * randY1.ToFloat() - 1.0;
        Float rSq1 = x1 * x1 + y1 * y1;
        if (rSq1 <= 1.0) {
            sumX = sumX + x1;
            sumY = sumY + y1;
            insideCount = insideCount + 1;
        }

        // Step 2
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int randX2 = (seed / 65536) % 1000;
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int randY2 = (seed / 65536) % 1000;

        Float x2 = 0.002 * randX2.ToFloat() - 1.0;
        Float y2 = 0.002 * randY2.ToFloat() - 1.0;
        Float rSq2 = x2 * x2 + y2 * y2;
        if (rSq2 <= 1.0) {
            sumX = sumX + x2;
            sumY = sumY + y2;
            insideCount = insideCount + 1;
        }

        i = i + 2;
    }

    return sumX + sumY + insideCount.ToFloat();
}

public Function Main() -> Void {
    Int n = 5000000;
    Float res = RunEP(n);
    Print("NPB EP Checksum:");
    Print(res);
}
