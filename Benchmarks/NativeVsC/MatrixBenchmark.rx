package Benchmarks.MatrixMul;

public Function MatMulFlat(Int n) -> Int {
    Int sum = 0;
    Int i = 0;
    while (i < n) {
        Int j = 0;
        while (j < n) {
            Int k = 0;
            while (k < n) {
                Int aVal = (i * 37 + k) % 100;
                Int bVal = (k * 19 + j) % 100;
                sum = sum + aVal * bVal;
                k = k + 1;
            }
            j = j + 1;
        }
        i = i + 1;
    }
    return sum;
}

public Function Main() -> Void {
    Int size = 250;
    Int total = MatMulFlat(size);
    Print("Robix Native Matrix Multiply Sum:");
    Print(total);
}
