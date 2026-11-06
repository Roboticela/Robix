package Examples.StageM;

public Function Main() -> Void {
    Array<Int> values = [1, 2, 3];
    Int total = 0;
    for value in values {
        total = total + value;
    }
    Print(total);
}
