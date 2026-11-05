package Benchmarks.KNucleotide;

public Function RunKNucleotide(Int n) -> Int {
    Int seed = 123456789;
    Int last = 0;
    
    // 16 2-mer counters
    Int c00 = 0; Int c01 = 0; Int c02 = 0; Int c03 = 0;
    Int c10 = 0; Int c11 = 0; Int c12 = 0; Int c13 = 0;
    Int c20 = 0; Int c21 = 0; Int c22 = 0; Int c23 = 0;
    Int c30 = 0; Int c31 = 0; Int c32 = 0; Int c33 = 0;

    Int i = 0;
    while (i < n) {
        // Step 1
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int curr = (seed / 65536) % 4;
        
        if (last == 0) {
            if (curr == 0) { c00 = c00 + 1; }
            else if (curr == 1) { c01 = c01 + 1; }
            else if (curr == 2) { c02 = c02 + 1; }
            else { c03 = c03 + 1; }
        } else if (last == 1) {
            if (curr == 0) { c10 = c10 + 1; }
            else if (curr == 1) { c11 = c11 + 1; }
            else if (curr == 2) { c12 = c12 + 1; }
            else { c13 = c13 + 1; }
        } else if (last == 2) {
            if (curr == 0) { c20 = c20 + 1; }
            else if (curr == 1) { c21 = c21 + 1; }
            else if (curr == 2) { c22 = c22 + 1; }
            else { c23 = c23 + 1; }
        } else {
            if (curr == 0) { c30 = c30 + 1; }
            else if (curr == 1) { c31 = c31 + 1; }
            else if (curr == 2) { c32 = c32 + 1; }
            else { c33 = c33 + 1; }
        }

        // Step 2
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int nextCurr = (seed / 65536) % 4;
        
        if (curr == 0) {
            if (nextCurr == 0) { c00 = c00 + 1; }
            else if (nextCurr == 1) { c01 = c01 + 1; }
            else if (nextCurr == 2) { c02 = c02 + 1; }
            else { c03 = c03 + 1; }
        } else if (curr == 1) {
            if (nextCurr == 0) { c10 = c10 + 1; }
            else if (nextCurr == 1) { c11 = c11 + 1; }
            else if (nextCurr == 2) { c12 = c12 + 1; }
            else { c13 = c13 + 1; }
        } else if (curr == 2) {
            if (nextCurr == 0) { c20 = c20 + 1; }
            else if (nextCurr == 1) { c21 = c21 + 1; }
            else if (nextCurr == 2) { c22 = c22 + 1; }
            else { c23 = c23 + 1; }
        } else {
            if (nextCurr == 0) { c30 = c30 + 1; }
            else if (nextCurr == 1) { c31 = c31 + 1; }
            else if (nextCurr == 2) { c32 = c32 + 1; }
            else { c33 = c33 + 1; }
        }

        last = nextCurr;
        i = i + 2;
    }

    Int checksum = (c00 * 1) + (c01 * 2) + (c02 * 3) + (c03 * 4) +
                   (c10 * 5) + (c11 * 6) + (c12 * 7) + (c13 * 8) +
                   (c20 * 9) + (c21 * 10) + (c22 * 11) + (c23 * 12) +
                   (c30 * 13) + (c31 * 14) + (c32 * 15) + (c33 * 16);
    return checksum;
}

public Function Main() -> Void {
    Int n = 10000000;
    Int res = RunKNucleotide(n);
    Print("Robix Native K-Nucleotide Result:");
    Print(res);
}
