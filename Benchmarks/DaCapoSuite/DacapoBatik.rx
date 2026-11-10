package Benchmarks.DaCapo.Batik;

public Function RunBatikSVG(Int numPaths) -> Float {
    // 2D Vector Graphics cubic Bezier spline evaluation
    // B(t) = (1-t)^3*P0 + 3(1-t)^2*t*P1 + 3(1-t)*t^2*P2 + t^3*P3
    Float p0X = 0.0; Float p0Y = 0.0;
    Float p1X = 10.0; Float p1Y = 30.0;
    Float p2X = 50.0; Float p2Y = 60.0;
    Float p3X = 100.0; Float p3Y = 10.0;

    Float sumX = 0.0;
    Float sumY = 0.0;

    Int i = 0;
    while (i < numPaths) {
        Float t = 0.5;
        Float oneMinusT = 1.0 - t;

        Float c0 = oneMinusT * oneMinusT * oneMinusT;
        Float c1 = 3.0 * oneMinusT * oneMinusT * t;
        Float c2 = 3.0 * oneMinusT * t * t;
        Float c3 = t * t * t;

        Float bx = c0 * p0X + c1 * p1X + c2 * p2X + c3 * p3X;
        Float by = c0 * p0Y + c1 * p1Y + c2 * p2Y + c3 * p3Y;

        sumX = sumX + bx;
        sumY = sumY + by;

        // Animate control points slightly
        p1X = p1X + 0.0001;
        p2Y = p2Y + 0.0001;

        i = i + 1;
    }

    return sumX + sumY;
}

public Function Main() -> Void {
    Int n = 10000000;
    Float res = RunBatikSVG(n);
    Print("DaCapo Batik Checksum:");
    Print(res);
}

