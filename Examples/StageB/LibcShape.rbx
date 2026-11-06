package Examples.StageB;

public struct Point {
    public Int x;
    public Int y;
}

public enum Color {
    Red,
    Green
}

extern Function puts(String text) -> Int32;

public Function Add(Int a, Int b) -> Int {
    return a + b;
}

public Function Add(Int a, Int b, Int c) -> Int {
    return a + b + c;
}

public Function Main() -> Void {
    let title = "Hel" + "lo";
    var steps = 1;
    const Int LIMIT = 8;

    Int two = Add(2, 3);
    Int three = Add(2, 3, 4);

    Array<Int> nums = [10, 20, 30];
    Int indexed = nums[1];
    Array<Int> head = nums[0..2];
    Int headSum = head[0] + head[1];

    Int walked = 0;
    for (Int i = 0; i < 3; i = i + 1) {
        walked = walked + nums[i];
        steps = steps + 1;
    }

    Int skipped = 0;
    Int n = 0;
    while (n < 5) {
        n = n + 1;
        if (n == 2) {
            continue;
        }
        if (n == 4) {
            break;
        }
        skipped = skipped + n;
    }

    Point p = new Point();
    p.x = 7;
    p.y = 8;

    Color tone = Color.Green;
    Int toneBit = 0;
    if (tone == Color.Green) {
        toneBit = 1;
    }

    Int32 small = 40;
    Int8 tiny = 3;
    UInt16 wide = 4;
    Byte seven = 7;

    Float ratio = 1.5;
    Int hot = 0;
    if (ratio > 1.0) {
        hot = 1;
    }

    Result<Int, String> ok = Success(5);
    Option<Int> some = Some(9);
    Option<Int> none = None();
    Tuple pair = Tuple(4, 6);

    Int speed = 100;
    Byte bit = 0;
    Unsafe {
        Pointer<Int> sp = &speed;
        *sp = 250;
        Pointer<Byte> raw = Memory.Allocate(8);
        Pointer<Byte> mid = raw + 1;
        *mid = seven;
        bit = *mid;
        Memory.Free(raw);
    }
    Int seen = bit.ToInt();

    Int q = 100 / 8;
    Int r = 100 % 8;
    Int neg = -9;
    Int nq = neg / 4;
    Int nr = neg % 4;
    Int dyn = 6;
    Int dq = 36 / dyn;

    Int same = 0;
    if (title == "Hello") {
        same = 1;
    }

    puts("ok");
    Print(title);

    Int checksum = two + three + indexed + headSum + walked + skipped + p.x + p.y + toneBit + hot + ok.value + some.value + some.tag + none.tag + pair.t0 + pair.t1 + speed + seen + q + r + nq + nr + dq + same + small.ToInt() + tiny.ToInt() + wide.ToInt() + steps + LIMIT;
    Print(checksum);
}
