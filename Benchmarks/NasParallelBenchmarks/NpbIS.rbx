package Benchmarks.NPB.IS;

public Function RunIS(Int numKeys) -> Int {
    Int seed = 314159265;

    Int b0 = 0; Int b1 = 0; Int b2 = 0; Int b3 = 0;
    Int b4 = 0; Int b5 = 0; Int b6 = 0; Int b7 = 0;
    Int b8 = 0; Int b9 = 0; Int b10 = 0; Int b11 = 0;
    Int b12 = 0; Int b13 = 0; Int b14 = 0; Int b15 = 0;

    Int i = 0;
    while (i < numKeys) {
        seed = (seed * 1220703125 + 1013904223) % 2147483648;
        Int key = (seed / 65536) % 16;

        if (key < 8) {
            if (key == 0) { b0 = b0 + 1; }
            else if (key == 1) { b1 = b1 + 1; }
            else if (key == 2) { b2 = b2 + 1; }
            else if (key == 3) { b3 = b3 + 1; }
            else if (key == 4) { b4 = b4 + 1; }
            else if (key == 5) { b5 = b5 + 1; }
            else if (key == 6) { b6 = b6 + 1; }
            else { b7 = b7 + 1; }
        } else {
            if (key == 8) { b8 = b8 + 1; }
            else if (key == 9) { b9 = b9 + 1; }
            else if (key == 10) { b10 = b10 + 1; }
            else if (key == 11) { b11 = b11 + 1; }
            else if (key == 12) { b12 = b12 + 1; }
            else if (key == 13) { b13 = b13 + 1; }
            else if (key == 14) { b14 = b14 + 1; }
            else { b15 = b15 + 1; }
        }

        i = i + 1;
    }

    Int checksum = b0 * 1 + b1 * 2 + b2 * 3 + b3 * 4 +
                   b4 * 5 + b5 * 6 + b6 * 7 + b7 * 8 +
                   b8 * 9 + b9 * 10 + b10 * 11 + b11 * 12 +
                   b12 * 13 + b13 * 14 + b14 * 15 + b15 * 16;

    return checksum;
}

public Function Main() -> Void {
    Int n = 10000000;
    Int res = RunIS(n);
    Print("NPB IS Checksum:");
    Print(res);
}
