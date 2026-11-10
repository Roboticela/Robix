package Examples.Language;

public interface Alpha {
    public Function FromAlpha() -> Int;
}

public interface Beta {
    public Function FromBeta() -> Int;
}

public class Pair implements Alpha, Beta {
    public Int seed;

    public Constructor() {
        this.seed = 40;
    }

    public override Function FromAlpha() -> Int {
        return this.seed + 1;
    }

    public override Function FromBeta() -> Int {
        return this.seed + 2;
    }
}

public Function Main() -> Void {
    Alpha first = new Pair();
    Beta second = new Pair();
    Print(first.FromAlpha());
    Print(second.FromBeta());
}
