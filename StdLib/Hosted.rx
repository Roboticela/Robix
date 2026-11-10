package StdLib.Hosted;

public Function Abs(Int x) -> Int {
    if (x < 0) {
        return 0 - x;
    }
    return x;
}

public Function Min(Int a, Int b) -> Int {
    if (a < b) {
        return a;
    }
    return b;
}

public Function Max(Int a, Int b) -> Int {
    if (a > b) {
        return a;
    }
    return b;
}

public Function Clamp(Int value, Int low, Int high) -> Int {
    return Min(Max(value, low), high);
}

public class IntList {
    private Int count;
    private Int slot0;
    private Int slot1;
    private Int slot2;
    private Int slot3;

    public Function Add(Int value) -> Void {
        if (this.count == 0) {
            this.slot0 = value;
        } else if (this.count == 1) {
            this.slot1 = value;
        } else if (this.count == 2) {
            this.slot2 = value;
        } else if (this.count == 3) {
            this.slot3 = value;
        }
        if (this.count < 4) {
            this.count = this.count + 1;
        }
    }

    public Function Get(Int index) -> Int {
        if (index == 0) {
            return this.slot0;
        } else if (index == 1) {
            return this.slot1;
        } else if (index == 2) {
            return this.slot2;
        } else if (index == 3) {
            return this.slot3;
        }
        return 0;
    }

    public Function Count() -> Int {
        return this.count;
    }
}

public Function Main() -> Void {
    IntList list = new IntList();
    list.Add(10);
    list.Add(20);

    Array<Int> nums = [1, 2, 3];
    String word = "Robix";

    Int total = list.Get(0) + list.Get(1) + nums.Length() + word.Length() + Min(3, 8) + Abs(0 - 4);
    Print(total);

    File.WriteText("Build/Hosted.txt", "hosted");
    Print(File.ReadText("Build/Hosted.txt"));

    Print(Math.Abs(0 - 4));
    Print(Math.Clamp(20, 0, 10));
    Print(Math.Max(2, 9));
}
