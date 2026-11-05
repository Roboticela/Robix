package Benchmarks.DaCapo.H2;

public Function RunH2Engine(Int numOps) -> Int {
    Int seed = 42424242;
    Int checksum = 0;

    // Simulate 4 tables: row counts & aggregate sums
    Int t0Rows = 0; Int t0Sum = 0;
    Int t1Rows = 0; Int t1Sum = 0;
    Int t2Rows = 0; Int t2Sum = 0;
    Int t3Rows = 0; Int t3Sum = 0;

    Int i = 0;
    while (i < numOps) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int opType = (seed / 65536) % 3; // 0: INSERT, 1: UPDATE, 2: SELECT
        Int table = (seed / 16384) % 4;
        Int val = (seed / 256) % 1000;

        if (opType == 0) {
            // INSERT
            if (table == 0) { t0Rows = t0Rows + 1; t0Sum = (t0Sum + val) % 1000000007; }
            else if (table == 1) { t1Rows = t1Rows + 1; t1Sum = (t1Sum + val) % 1000000007; }
            else if (table == 2) { t2Rows = t2Rows + 1; t2Sum = (t2Sum + val) % 1000000007; }
            else { t3Rows = t3Rows + 1; t3Sum = (t3Sum + val) % 1000000007; }
        } else if (opType == 1) {
            // UPDATE
            if (table == 0) { t0Sum = (t0Sum + val) % 1000000007; }
            else if (table == 1) { t1Sum = (t1Sum + val) % 1000000007; }
            else if (table == 2) { t2Sum = (t2Sum + val) % 1000000007; }
            else { t3Sum = (t3Sum + val) % 1000000007; }
        } else {
            // SELECT
            Int querySum = 0;
            if (table == 0) { querySum = t0Sum + t0Rows; }
            else if (table == 1) { querySum = t1Sum + t1Rows; }
            else if (table == 2) { querySum = t2Sum + t2Rows; }
            else { querySum = t3Sum + t3Rows; }
            checksum = (checksum * 31 + querySum) % 1000000007;
        }

        i = i + 1;
    }

    checksum = (checksum + t0Sum + t1Sum + t2Sum + t3Sum) % 1000000007;
    return checksum;
}

public Function Main() -> Void {
    Int n = 5000000;
    Int res = RunH2Engine(n);
    Print("DaCapo H2 Checksum:");
    Print(res);
}

