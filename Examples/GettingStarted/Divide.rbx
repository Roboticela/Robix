package Examples.GettingStarted;

public Function Divide(Int a, Int b) -> Result {
    if (b == 0) {
        return Error(0);
    }
    return Success(a / b);
}

public Function Main() -> Void {
    Result bad = Divide(1, 0);
    Result ok = Divide(20, 4);
    Print(bad.tag);
    Print(ok.value);
}
