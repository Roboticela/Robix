package Examples.StageN;

public interface One {
    public Function Mid() -> Int;
}

public interface Extra {
    public Function Aaa() -> Int;
}

public class Plain implements One {
    public override Function Mid() -> Int {
        return 11;
    }
}

public class WithExtra implements One, Extra {
    public override Function Aaa() -> Int {
        return 1;
    }

    public override Function Mid() -> Int {
        return 22;
    }
}

public Function Main() -> Void {
    One first = new Plain();
    One second = new WithExtra();
    Print(first.Mid());
    Print(second.Mid());
}
