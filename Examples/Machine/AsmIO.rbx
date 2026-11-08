package Examples.Machine;

public Function Main() -> Void {
    Int x = 20;
    Int y = 0;
    Unsafe {
        ASM("movq $1, $0", out: y, in: x);
    }
    Print(y);
}
