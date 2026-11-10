package Benchmarks.Pidigits;

public Function ModExp16(Int exp, Int mod) -> Int {
    Int res = 1;
    Int b = 16 % mod;
    Int e = exp;
    while (e > 0) {
        if (e % 2 == 1) {
            res = (res * b) % mod;
        }
        b = (b * b) % mod;
        e = e / 2;
    }
    return res;
}

public Function ComputePiDigit(Int n) -> Int {
    Float s1 = 0.0;
    Float s4 = 0.0;
    Float s5 = 0.0;
    Float s6 = 0.0;

    Int k = 0;
    while (k <= n) {
        Int exp = n - k;
        Int d1 = 8 * k + 1;
        Int d4 = d1 + 3;
        Int d5 = d1 + 4;
        Int d6 = d1 + 5;

        Float fd1 = d1.ToFloat();
        s1 = s1 + ModExp16(exp, d1).ToFloat() / fd1;
        s4 = s4 + ModExp16(exp, d4).ToFloat() / (fd1 + 3.0);
        s5 = s5 + ModExp16(exp, d5).ToFloat() / (fd1 + 4.0);
        s6 = s6 + ModExp16(exp, d6).ToFloat() / (fd1 + 5.0);

        k = k + 1;
    }

    s1 = s1 - s1.ToInt().ToFloat();
    s4 = s4 - s4.ToInt().ToFloat();
    s5 = s5 - s5.ToInt().ToFloat();
    s6 = s6 - s6.ToInt().ToFloat();

    k = n + 1;
    Float p16 = 0.0625;
    while (k <= n + 3) {
        Float fdk = (8 * k + 1).ToFloat();
        s1 = s1 + p16 / fdk;
        s4 = s4 + p16 / (fdk + 3.0);
        s5 = s5 + p16 / (fdk + 4.0);
        s6 = s6 + p16 / (fdk + 5.0);
        p16 = p16 * 0.0625;
        k = k + 1;
    }

    Float x = 4.0 * s1 - 2.0 * s4 - s5 - s6;
    x = x - x.ToInt().ToFloat();
    if (x < 0.0) {
        x = x + 1.0;
    }
    Int digit = (x * 16.0).ToInt() % 16;
    return digit;
}

public Function RunPidigits(Int numDigits) -> Int {
    Int checksum = 0;
    Int i = 0;
    while (i < numDigits) {
        Int d = ComputePiDigit(i);
        checksum = (checksum * 13 + d) % 1000000007;
        i = i + 1;
    }
    return checksum;
}

public Function Main() -> Void {
    Int n = 500;
    Int res = RunPidigits(n);
    Print("Pidigits Checksum:");
    Print(res);
}
