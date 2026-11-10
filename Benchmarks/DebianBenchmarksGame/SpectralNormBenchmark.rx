package Benchmarks.SpectralNorm;

public Function RunSpectralNorm(Int n) -> Float {
    Float sum = 0.0;
    Int i = 0;
    while (i < n) {
        Float fi = i.ToFloat();
        Int j = 0;
        Float rowSum = 0.0;
        while (j < n) {
            Float fij = fi + j.ToFloat();
            Float d = (fij * (fij + 1.0)) * 0.5 + fi + 1.0;
            rowSum = rowSum + 1.0 / d;
            j = j + 1;
        }
        Float fii = fi + fi;
        Float dii = (fii * (fii + 1.0)) * 0.5 + fi + 1.0;
        sum = sum + rowSum / dii;
        i = i + 1;
    }
    return sum;
}

public Function Main() -> Void {
    Int n = 2500;
    Float res = RunSpectralNorm(n);
    Print("Robix Native Spectral-Norm Result:");
    Print(res);
}
