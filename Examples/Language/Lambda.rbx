package Examples.Language;

public Function Main() -> Void {
    Int base = 40;
    Int closed = Function(step) { return step + base; }(2);
    Print(closed);
}
