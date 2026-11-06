package Examples.StageF;

public Function Main() -> Void {
    Int acc = 0;
    for (Int i = 0; i < 50000000; i = i + 1) {
        acc = acc + ((i ^ (i >> 1)) & 65535);
    }
    Print(acc);
}
