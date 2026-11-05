package Benchmarks.SpectralNorm;

public Function Main() -> Void {
    Int n = 3000;
    Float sum = 0.0;
    Int i = 0;
    while (i < n) {
        Int j = 0;
        while (j < n) {
            Int ij = (i + j) * (i + j + 1) / 2 + i + 1;
            Float elem = 1.0;
            Float denom = 0.0;
            Int k = 0;
            while (k < ij) {
                denom = denom + 1.0;
                k = k + 1000;
            }
            sum = sum + elem / (Float)ij;
            j = j + 1;
        }
        i = i + 1;
    }
    Print("Robix Native Spectral Element Sum:");
    Print(sum);
}
