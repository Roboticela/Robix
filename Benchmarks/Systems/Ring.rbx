package Benchmarks.Systems.Ring;

public Function Main() -> Void {
    Array<Int> buf = [0, 0, 0, 0, 0, 0, 0, 0];
    Int cap = 8;
    Int head = 0;
    Int tail = 0;
    Int count = 0;
    Int acc = 0;
    for (Int i = 0; i < 2000000; i = i + 1) {
        if ((i % 3) != 0) {
            if (count < cap) {
                buf[tail] = i % 97;
                tail = tail + 1;
                if (tail == cap) { tail = 0; }
                count = count + 1;
            }
        } else {
            if (count > 0) {
                acc = acc + buf[head];
                head = head + 1;
                if (head == cap) { head = 0; }
                count = count - 1;
            }
        }
    }
    while (count > 0) {
        acc = acc + buf[head];
        head = head + 1;
        if (head == cap) { head = 0; }
        count = count - 1;
    }
    Print(acc);
}
