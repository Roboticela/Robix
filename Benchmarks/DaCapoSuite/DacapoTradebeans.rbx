package Benchmarks.DaCapo.Tradebeans;

public Function RunTradebeansMarket(Int numTrades) -> Int {
    Int b0 = 100; Int b1 = 99; Int b2 = 98; Int b3 = 97;
    Int s0 = 101; Int s1 = 102; Int s2 = 103; Int s3 = 104;

    Int seed = 555444333;
    Int totalExecutedVol = 0;
    Int totalTurnover = 0;

    Int i = 0;
    while (i < numTrades) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        Int orderSide = (seed / 65536) % 2;
        Int price = 95 + ((seed / 16384) % 12);
        Int qty = ((seed / 256) % 10) + 1;

        if (orderSide == 0) {
            if (price >= s0) {
                totalExecutedVol = totalExecutedVol + qty;
                totalTurnover = totalTurnover + qty * s0;
                if (totalTurnover >= 1000000007) {
                    totalTurnover = totalTurnover - 1000000007;
                }
                s0 = s0 + 1;
            } else {
                b0 = price;
            }
        } else {
            if (price <= b0) {
                totalExecutedVol = totalExecutedVol + qty;
                totalTurnover = totalTurnover + qty * b0;
                if (totalTurnover >= 1000000007) {
                    totalTurnover = totalTurnover - 1000000007;
                }
                b0 = b0 - 1;
            } else {
                s0 = price;
            }
        }

        i = i + 1;
    }

    return (totalTurnover + totalExecutedVol + b0 + s0) % 1000000007;
}

public Function Main() -> Void {
    Int n = 5000000;
    Int res = RunTradebeansMarket(n);
    Print("DaCapo Tradebeans Checksum:");
    Print(res);
}
