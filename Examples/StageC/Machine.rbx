package Examples.StageC;

public interface Drive {
    public Function Power() -> Int;
}

public class Walker implements Drive {
    public override Function Power() -> Int {
        return 10;
    }
}

public class Runner extends Walker {
    public override Function Power() -> Int {
        return 30;
    }
}

public class Motor {
    private Int speed;

    public Constructor() {
        this.speed = 4;
    }

    public Function SetSpeed(Int value) -> Void {
        this.speed = value;
    }

    public Function GetSpeed() -> Int {
        return this.speed;
    }

    public Destructor() {
        Print(this.speed);
    }
}

public class Box<T> {
    public T item;

    public Function Set(T value) -> Void {
        this.item = value;
    }

    public Function Get() -> T {
        return this.item;
    }
}

public Function Leaf(Int x) -> Int {
    return x + 1;
}

public Function MakeRunner() -> Walker {
    return new Runner();
}

public Function Main() -> Void {
    Motor motor = new Motor();
    motor.SetSpeed(40);

    Drive walk = new Walker();
    Drive run = new Runner();
    Walker paced = new Runner();

    Box<Int> box = new Box<Int>();
    box.Set(7);

    Walker hidden = MakeRunner();

    Int total = motor.GetSpeed() + walk.Power() + run.Power() + paced.Power() + box.Get() + hidden.Power() + Leaf(2);
    Print(total);
}
