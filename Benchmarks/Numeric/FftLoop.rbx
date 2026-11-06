package Benchmarks.Numeric.FftLoop;

public Function Main() -> Void {
    Array<Int> buf = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    for (Int i = 0; i < 16; i = i + 1) {
        buf[i] = (i * 9 + 1) % 10007;
    }
    for (Int rep = 0; rep < 200000; rep = rep + 1) {
        Int len = 1;
        while (len < 16) {
            Int i = 0;
            while (i < 16) {
                for (Int j = 0; j < len; j = j + 1) {
                    Int u = buf[i + j];
                    Int v = buf[i + j + len];
                    buf[i + j] = (u + v) % 10007;
                    Int diff = u - v;
                    if (diff < 0) {
                        diff = diff + 10007;
                    }
                    buf[i + j + len] = diff % 10007;
                }
                i = i + len + len;
            }
            len = len + len;
        }
    }
    Int sum = 0;
    for (Int i = 0; i < 16; i = i + 1) {
        sum = sum + buf[i];
    }
    Print(sum);
}
