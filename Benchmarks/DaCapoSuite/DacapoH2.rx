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
        Int opType = (seed / 65536) % 3;
        Int table = (seed / 16384) % 4;
        Int val = (seed / 256) % 1000;

        if (opType == 0) {
            if (table == 0) {
                t0Rows = t0Rows + 1;
                t0Sum = t0Sum + val;
                if (t0Sum >= 1000000007) { t0Sum = t0Sum - 1000000007; }
            } else if (table == 1) {
                t1Rows = t1Rows + 1;
                t1Sum = t1Sum + val;
                if (t1Sum >= 1000000007) { t1Sum = t1Sum - 1000000007; }
            } else if (table == 2) {
                t2Rows = t2Rows + 1;
                t2Sum = t2Sum + val;
                if (t2Sum >= 1000000007) { t2Sum = t2Sum - 1000000007; }
            } else {
                t3Rows = t3Rows + 1;
                t3Sum = t3Sum + val;
                if (t3Sum >= 1000000007) { t3Sum = t3Sum - 1000000007; }
            }
        } else if (opType == 1) {
            if (table == 0) {
                t0Sum = t0Sum + val;
                if (t0Sum >= 1000000007) { t0Sum = t0Sum - 1000000007; }
            } else if (table == 1) {
                t1Sum = t1Sum + val;
                if (t1Sum >= 1000000007) { t1Sum = t1Sum - 1000000007; }
            } else if (table == 2) {
                t2Sum = t2Sum + val;
                if (t2Sum >= 1000000007) { t2Sum = t2Sum - 1000000007; }
            } else {
                t3Sum = t3Sum + val;
                if (t3Sum >= 1000000007) { t3Sum = t3Sum - 1000000007; }
            }
        } else {
            Int querySum = 0;
            if (table == 0) { querySum = t0Sum + t0Rows; }
            else if (table == 1) { querySum = t1Sum + t1Rows; }
            else if (table == 2) { querySum = t2Sum + t2Rows; }
            else { querySum = t3Sum + t3Rows; }
            checksum = (checksum * 31 + querySum) % 1000000007;
        }

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int opType2 = (seed / 65536) % 3;
        Int table2 = (seed / 16384) % 4;
        Int val2 = (seed / 256) % 1000;

        if (opType2 == 0) {
            if (table2 == 0) {
                t0Rows = t0Rows + 1;
                t0Sum = t0Sum + val2;
                if (t0Sum >= 1000000007) { t0Sum = t0Sum - 1000000007; }
            } else if (table2 == 1) {
                t1Rows = t1Rows + 1;
                t1Sum = t1Sum + val2;
                if (t1Sum >= 1000000007) { t1Sum = t1Sum - 1000000007; }
            } else if (table2 == 2) {
                t2Rows = t2Rows + 1;
                t2Sum = t2Sum + val2;
                if (t2Sum >= 1000000007) { t2Sum = t2Sum - 1000000007; }
            } else {
                t3Rows = t3Rows + 1;
                t3Sum = t3Sum + val2;
                if (t3Sum >= 1000000007) { t3Sum = t3Sum - 1000000007; }
            }
        } else if (opType2 == 1) {
            if (table2 == 0) {
                t0Sum = t0Sum + val2;
                if (t0Sum >= 1000000007) { t0Sum = t0Sum - 1000000007; }
            } else if (table2 == 1) {
                t1Sum = t1Sum + val2;
                if (t1Sum >= 1000000007) { t1Sum = t1Sum - 1000000007; }
            } else if (table2 == 2) {
                t2Sum = t2Sum + val2;
                if (t2Sum >= 1000000007) { t2Sum = t2Sum - 1000000007; }
            } else {
                t3Sum = t3Sum + val2;
                if (t3Sum >= 1000000007) { t3Sum = t3Sum - 1000000007; }
            }
        } else {
            Int querySum2 = 0;
            if (table2 == 0) { querySum2 = t0Sum + t0Rows; }
            else if (table2 == 1) { querySum2 = t1Sum + t1Rows; }
            else if (table2 == 2) { querySum2 = t2Sum + t2Rows; }
            else { querySum2 = t3Sum + t3Rows; }
            checksum = (checksum * 31 + querySum2) % 1000000007;
        }

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int opType3 = (seed / 65536) % 3;
        Int table3 = (seed / 16384) % 4;
        Int val3 = (seed / 256) % 1000;

        if (opType3 == 0) {
            if (table3 == 0) {
                t0Rows = t0Rows + 1;
                t0Sum = t0Sum + val3;
                if (t0Sum >= 1000000007) { t0Sum = t0Sum - 1000000007; }
            } else if (table3 == 1) {
                t1Rows = t1Rows + 1;
                t1Sum = t1Sum + val3;
                if (t1Sum >= 1000000007) { t1Sum = t1Sum - 1000000007; }
            } else if (table3 == 2) {
                t2Rows = t2Rows + 1;
                t2Sum = t2Sum + val3;
                if (t2Sum >= 1000000007) { t2Sum = t2Sum - 1000000007; }
            } else {
                t3Rows = t3Rows + 1;
                t3Sum = t3Sum + val3;
                if (t3Sum >= 1000000007) { t3Sum = t3Sum - 1000000007; }
            }
        } else if (opType3 == 1) {
            if (table3 == 0) {
                t0Sum = t0Sum + val3;
                if (t0Sum >= 1000000007) { t0Sum = t0Sum - 1000000007; }
            } else if (table3 == 1) {
                t1Sum = t1Sum + val3;
                if (t1Sum >= 1000000007) { t1Sum = t1Sum - 1000000007; }
            } else if (table3 == 2) {
                t2Sum = t2Sum + val3;
                if (t2Sum >= 1000000007) { t2Sum = t2Sum - 1000000007; }
            } else {
                t3Sum = t3Sum + val3;
                if (t3Sum >= 1000000007) { t3Sum = t3Sum - 1000000007; }
            }
        } else {
            Int querySum3 = 0;
            if (table3 == 0) { querySum3 = t0Sum + t0Rows; }
            else if (table3 == 1) { querySum3 = t1Sum + t1Rows; }
            else if (table3 == 2) { querySum3 = t2Sum + t2Rows; }
            else { querySum3 = t3Sum + t3Rows; }
            checksum = (checksum * 31 + querySum3) % 1000000007;
        }

        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int opType4 = (seed / 65536) % 3;
        Int table4 = (seed / 16384) % 4;
        Int val4 = (seed / 256) % 1000;

        if (opType4 == 0) {
            if (table4 == 0) {
                t0Rows = t0Rows + 1;
                t0Sum = t0Sum + val4;
                if (t0Sum >= 1000000007) { t0Sum = t0Sum - 1000000007; }
            } else if (table4 == 1) {
                t1Rows = t1Rows + 1;
                t1Sum = t1Sum + val4;
                if (t1Sum >= 1000000007) { t1Sum = t1Sum - 1000000007; }
            } else if (table4 == 2) {
                t2Rows = t2Rows + 1;
                t2Sum = t2Sum + val4;
                if (t2Sum >= 1000000007) { t2Sum = t2Sum - 1000000007; }
            } else {
                t3Rows = t3Rows + 1;
                t3Sum = t3Sum + val4;
                if (t3Sum >= 1000000007) { t3Sum = t3Sum - 1000000007; }
            }
        } else if (opType4 == 1) {
            if (table4 == 0) {
                t0Sum = t0Sum + val4;
                if (t0Sum >= 1000000007) { t0Sum = t0Sum - 1000000007; }
            } else if (table4 == 1) {
                t1Sum = t1Sum + val4;
                if (t1Sum >= 1000000007) { t1Sum = t1Sum - 1000000007; }
            } else if (table4 == 2) {
                t2Sum = t2Sum + val4;
                if (t2Sum >= 1000000007) { t2Sum = t2Sum - 1000000007; }
            } else {
                t3Sum = t3Sum + val4;
                if (t3Sum >= 1000000007) { t3Sum = t3Sum - 1000000007; }
            }
        } else {
            Int querySum4 = 0;
            if (table4 == 0) { querySum4 = t0Sum + t0Rows; }
            else if (table4 == 1) { querySum4 = t1Sum + t1Rows; }
            else if (table4 == 2) { querySum4 = t2Sum + t2Rows; }
            else { querySum4 = t3Sum + t3Rows; }
            checksum = (checksum * 31 + querySum4) % 1000000007;
        }

        i = i + 4;
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
