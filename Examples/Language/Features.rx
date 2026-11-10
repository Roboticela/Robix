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
    Array<Int> values = [1, 2, 3];
    Int total = 0;
    for item in values {
        total = total + item;
    }
    Print(total);

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

    Int caught = 0;
    try {
        throw 7;
    } catch (code) {
        caught = code;
    }
    Print(caught);

    {
        defer {
            Print(11);
        }
        Print(10);
    }

    Int base = 40;
    Int closed = Function(step) { return step + base; }(2);
    Print(closed);

    using Int held = 12 {
        Print(held);
    }

    Alpha first = new Pair();
    Beta second = new Pair();
    Print(first.FromAlpha());
    Print(second.FromBeta());

    Int pwm = PWM.SetDuty(18);
    Int uart = UART.Write(1);
    Int spi = SPI.Transfer(2);
    Int i2c = I2C.Write(3);
    Print(pwm + uart + spi + i2c);
}
