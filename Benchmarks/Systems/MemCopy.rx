package Benchmarks.Systems.MemCopy;

public Function Main() -> Void {
    Array<Int> src = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    Array<Int> dst = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    for (Int i = 0; i < 16; i = i + 1) {
        src[i] = (i * 3 + 1) % 97;
    }
    for (Int rep = 0; rep < 200000; rep = rep + 1) {
        for (Int i = 0; i < 16; i = i + 1) {
            dst[i] = src[i];
        }
    }
    Int sum = 0;
    for (Int i = 0; i < 16; i = i + 1) {
        sum = sum + dst[i];
    }
    Print(sum);
}
