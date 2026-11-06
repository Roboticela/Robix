package Examples.StageD;

public struct Point {
    public Int x;
    public Int y;
}

public Function Main() -> Void {
    Point p = new Point();
    p.x = 3;
    p.y = 4;
    Print(p.x + p.y);
}
