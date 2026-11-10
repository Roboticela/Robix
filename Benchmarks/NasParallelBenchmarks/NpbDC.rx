package Benchmarks.NPB.DC;

public Function RunDC(Int numRecords) -> Int {
    Int seed = 577215664;
    Int checksum = 0;

    Int agg0 = 0; Int agg1 = 0; Int agg2 = 0; Int agg3 = 0;
    Int agg4 = 0; Int agg5 = 0; Int agg6 = 0; Int agg7 = 0;

    Int i = 0;
    while (i < numRecords) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int d0 = (seed / 65536) % 4;
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int d1 = (seed / 65536) % 2;
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int measure = (seed / 65536) % 100;

        Int slot = d0 * 2 + d1;
        if (slot < 4) {
            if (slot == 0) { agg0 = agg0 + measure; if (agg0 >= 1000000007) { agg0 = agg0 - 1000000007; } }
            else if (slot == 1) { agg1 = agg1 + measure; if (agg1 >= 1000000007) { agg1 = agg1 - 1000000007; } }
            else if (slot == 2) { agg2 = agg2 + measure; if (agg2 >= 1000000007) { agg2 = agg2 - 1000000007; } }
            else { agg3 = agg3 + measure; if (agg3 >= 1000000007) { agg3 = agg3 - 1000000007; } }
        } else {
            if (slot == 4) { agg4 = agg4 + measure; if (agg4 >= 1000000007) { agg4 = agg4 - 1000000007; } }
            else if (slot == 5) { agg5 = agg5 + measure; if (agg5 >= 1000000007) { agg5 = agg5 - 1000000007; } }
            else if (slot == 6) { agg6 = agg6 + measure; if (agg6 >= 1000000007) { agg6 = agg6 - 1000000007; } }
            else { agg7 = agg7 + measure; if (agg7 >= 1000000007) { agg7 = agg7 - 1000000007; } }
        }

        i = i + 1;
    }

    checksum = (agg0 * 1 + agg1 * 2 + agg2 * 3 + agg3 * 4 +
                agg4 * 5 + agg5 * 6 + agg6 * 7 + agg7 * 8) % 1000000007;

    return checksum;
}

public Function Main() -> Void {
    Int n = 5000000;
    Int res = RunDC(n);
    Print("NPB DC Checksum:");
    Print(res);
}
