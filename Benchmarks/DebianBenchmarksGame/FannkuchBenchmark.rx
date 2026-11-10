package Benchmarks.FannkuchRedux;

public Function FlipCount(Int perm) -> Int {
    Int p = perm;
    Int first = p % 16;
    if (first == 1) {
        return 0;
    }
    Int flips = 0;
    while (first > 1) {
        if (first == 2) {
            Int n0 = p % 16;
            Int n1 = (p / 16) % 16;
            p = p - (n1 - n0) * 15;
        } else if (first == 3) {
            Int n0 = p % 16;
            Int n2 = (p / 256) % 16;
            p = p - (n2 - n0) * 255;
        } else if (first == 4) {
            Int n0 = p % 16;
            Int n1 = (p / 16) % 16;
            Int n2 = (p / 256) % 16;
            Int n3 = (p / 4096) % 16;
            p = p - (n3 - n0) * 4095 - (n2 - n1) * 240;
        } else if (first == 5) {
            Int n0 = p % 16;
            Int n1 = (p / 16) % 16;
            Int n3 = (p / 4096) % 16;
            Int n4 = (p / 65536) % 16;
            p = p - (n4 - n0) * 65535 - (n3 - n1) * 3840;
        } else if (first == 6) {
            Int n0 = p % 16;
            Int n1 = (p / 16) % 16;
            Int n2 = (p / 256) % 16;
            Int n3 = (p / 4096) % 16;
            Int n4 = (p / 65536) % 16;
            Int n5 = (p / 1048576) % 16;
            p = p - (n5 - n0) * 1048575 - (n4 - n1) * 61440 - (n3 - n2) * 3840;
        } else if (first == 7) {
            Int n0 = p % 16;
            Int n1 = (p / 16) % 16;
            Int n2 = (p / 256) % 16;
            Int n4 = (p / 65536) % 16;
            Int n5 = (p / 1048576) % 16;
            Int n6 = (p / 16777216) % 16;
            p = p - (n6 - n0) * 16777215 - (n5 - n1) * 983040 - (n4 - n2) * 61440;
        } else {
            Int n0 = p % 16;
            Int n1 = (p / 16) % 16;
            Int n2 = (p / 256) % 16;
            Int n3 = (p / 4096) % 16;
            Int n4 = (p / 65536) % 16;
            Int n5 = (p / 1048576) % 16;
            Int n6 = (p / 16777216) % 16;
            Int n7 = (p / 268435456) % 16;
            p = p - (n7 - n0) * 268435455 - (n6 - n1) * 15728640 - (n5 - n2) * 983040 - (n4 - n3) * 61440;
        }
        flips = flips + 1;
        first = p % 16;
    }
    return flips;
}

public Function MakePerm(Int r) -> Int {
    Int p = (((r + 7) % 8) + 1) +
            ((((r + 14) % 8) + 1) * 16) +
            ((((r + 21) % 8) + 1) * 256) +
            ((((r + 28) % 8) + 1) * 4096) +
            ((((r + 35) % 8) + 1) * 65536) +
            ((((r + 42) % 8) + 1) * 1048576) +
            ((((r + 49) % 8) + 1) * 16777216) +
            ((((r + 56) % 8) + 1) * 268435456);
    return p;
}

public Function RunFannkuch(Int n) -> Int {
    Int maxFlips = 0;
    Int checksum = 0;
    Int count = 1000000;
    Int i = 0;
    Int seed = 123456789;
    while (i < count) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int r1 = seed % 40320;
        Int p1 = MakePerm(r1);
        Int flips1 = FlipCount(p1);
        if (flips1 > maxFlips) {
            maxFlips = flips1;
        }
        checksum = checksum + flips1;

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int r2 = seed % 40320;
        Int p2 = MakePerm(r2);
        Int flips2 = FlipCount(p2);
        if (flips2 > maxFlips) {
            maxFlips = flips2;
        }
        checksum = checksum - flips2;

        i = i + 2;
    }
    return checksum + maxFlips;
}

public Function Main() -> Void {
    Int res = RunFannkuch(8);
    Print("Robix Native Fannkuch-Redux Result:");
    Print(res);
}
