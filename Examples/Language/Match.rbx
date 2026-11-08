package Examples.Language;

public Function Main() -> Void {
    Result bad = Error(3);
    Result ok = Success(9);
    match bad {
        Success(found) => Print(found),
        Error(code) => Print(code)
    }
    match ok {
        Success(found) => Print(found),
        Error(code) => Print(code)
    }
}
