package Tests.Conformance;

public struct Point {
    public Int x;
    public Int y;
}

Test native "structs" {
    Point p = new Point();
    p.x = 7;
    p.y = 8;
    Assert.Equal(p.x + p.y, 15);
}
