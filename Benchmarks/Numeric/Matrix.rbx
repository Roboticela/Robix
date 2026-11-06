package Benchmarks.Numeric.Matrix;

public Function Main() -> Void {
    Array<Int> a = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    Array<Int> b = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    for (Int i = 0; i < 16; i = i + 1) {
        a[i] = (i * 3 + 1) % 97;
        b[i] = (i * 5 + 2) % 89;
    }
    Int total = 0;
    for (Int rep = 0; rep < 30000; rep = rep + 1) {
        for (Int row = 0; row < 4; row = row + 1) {
            for (Int col = 0; col < 4; col = col + 1) {
                Int s = 0;
                for (Int k = 0; k < 4; k = k + 1) {
                    s = s + a[row * 4 + k] * b[k * 4 + col];
                }
                total = total + s;
            }
        }
    }
    Print(total);
}
