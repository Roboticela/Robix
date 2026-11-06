package Examples.StageM;

public Function Main() -> Void {
    Int caught = 0;
    try {
        throw 7;
    } catch (code) {
        caught = code;
    }
    Print(caught);
}
