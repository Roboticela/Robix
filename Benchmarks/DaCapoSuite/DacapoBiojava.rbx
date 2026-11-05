package Benchmarks.DaCapo.Biojava;

public Function RunBiojavaNeedlemanWunsch(Int seqLen) -> Int {
    // 4-state dynamic programming sequence alignment
    // Match: +2, Mismatch: -1, Gap: -2
    Int dp0 = 0; Int dp1 = -2; Int dp2 = -4; Int dp3 = -6;

    Int seedA = 1234567;
    Int seedB = 7654321;

    Int i = 0;
    while (i < seqLen) {
        // Step 1
        seedA = (seedA * 1103515245 + 12345) % 2147483648;
        seedB = (seedB * 1103515245 + 12345) % 2147483648;
        Int charA0 = (seedA / 65536) % 4;
        Int charB0 = (seedB / 65536) % 4;

        Int matchScore0 = -1;
        if (charA0 == charB0) { matchScore0 = 2; }

        Int diag0 = dp0 + matchScore0;
        Int up0 = dp1 - 2;
        Int left0 = dp2 - 2;

        Int nextDp0 = diag0;
        if (up0 > nextDp0) { nextDp0 = up0; }
        if (left0 > nextDp0) { nextDp0 = left0; }

        dp0 = dp1;
        dp1 = dp2;
        dp2 = dp3;
        dp3 = nextDp0;

        // Step 2
        seedA = (seedA * 1103515245 + 12345) % 2147483648;
        seedB = (seedB * 1103515245 + 12345) % 2147483648;
        Int charA1 = (seedA / 65536) % 4;
        Int charB1 = (seedB / 65536) % 4;

        Int matchScore1 = -1;
        if (charA1 == charB1) { matchScore1 = 2; }

        Int diag1 = dp0 + matchScore1;
        Int up1 = dp1 - 2;
        Int left1 = dp2 - 2;

        Int nextDp1 = diag1;
        if (up1 > nextDp1) { nextDp1 = up1; }
        if (left1 > nextDp1) { nextDp1 = left1; }

        dp0 = dp1;
        dp1 = dp2;
        dp2 = dp3;
        dp3 = nextDp1;

        i = i + 2;
    }

    return dp0 + dp1 + dp2 + dp3;
}

public Function Main() -> Void {
    Int n = 10000000;
    Int res = RunBiojavaNeedlemanWunsch(n);
    Print("DaCapo Biojava Checksum:");
    Print(res);
}
