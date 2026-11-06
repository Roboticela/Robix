package Benchmarks.Numeric.Particle;

public Function Main() -> Void {
    Int x = 0;
    Int y = 0;
    Int vx = 3;
    Int vy = 0 - 1;
    for (Int i = 0; i < 2000000; i = i + 1) {
        vx = vx + (i % 5) - 2;
        vy = vy + (i % 3) - 1;
        if (vx > 20) { vx = 20; }
        if (vx < 0 - 20) { vx = 0 - 20; }
        if (vy > 20) { vy = 20; }
        if (vy < 0 - 20) { vy = 0 - 20; }
        x = x + vx;
        y = y + vy;
        if (x > 100000) { x = x - 100000; }
        if (x < 0) { x = x + 100000; }
        if (y > 100000) { y = y - 100000; }
        if (y < 0) { y = y + 100000; }
    }
    Print(x + y + vx + vy);
}
