package Benchmarks.Mandelbrot;

public Function ComputeMandelbrotPoint(Float cr, Float ci, Int maxIter) -> Int {
    Float crMinus = cr - 0.25;
    Float ci2Val = ci * ci;
    Float q = crMinus * crMinus + ci2Val;
    if (q * (q + crMinus) <= 0.25 * ci2Val) {
        return maxIter;
    }
    Float crPlus = cr + 1.0;
    if (crPlus * crPlus + ci2Val <= 0.0625) {
        return maxIter;
    }

    Float zr = 0.0;
    Float zi = 0.0;
    Int iter = 0;
    while (iter < maxIter) {
        Float zr2 = zr * zr;
        Float zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            return iter;
        }
        Float ziNext = 2.0 * zr * zi + ci;
        Float zrNext = zr2 - zi2 + cr;

        Float zr2Next = zrNext * zrNext;
        Float zi2Next = ziNext * ziNext;
        if (zr2Next + zi2Next > 4.0) {
            return iter + 1;
        }
        zi = 2.0 * zrNext * ziNext + ci;
        zr = zr2Next - zi2Next + cr;
        iter = iter + 2;
    }
    return maxIter;
}

public Function Main() -> Void {
    Int width = 500;
    Int height = 500;
    Int totalEscaped = 0;

    Float rMin = 0.0 - 2.0;
    Float rMax = 1.0;
    Float iMin = 0.0 - 1.5;
    Float iMax = 1.5;

    Float rStep = (rMax - rMin) / 500.0;
    Float iStep = (iMax - iMin) / 500.0;

    Int y = 0;
    Float ci = iMin;
    while (y < height) {
        Int x = 0;
        Float cr = rMin;
        while (x < width) {
            Int iters = ComputeMandelbrotPoint(cr, ci, 100);
            if (iters < 100) {
                totalEscaped = totalEscaped + 1;
            }
            cr = cr + rStep;
            x = x + 1;
        }
        ci = ci + iStep;
        y = y + 1;
    }

    Print("Robix Native Mandelbrot Total Escaped Pixels:");
    Print(totalEscaped);
}

