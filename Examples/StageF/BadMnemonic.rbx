package Examples.StageF;

public Function Start() -> Void {
    Unsafe {
        ASM("outb $82, %al");
    }
}
