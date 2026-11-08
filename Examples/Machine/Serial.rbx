package Examples.Machine;

public Int PageEntry(Int phys, Int flags) {
    return (phys & ~0xFFF) | flags;
}

public Int SwitchChecksum(Int savedSp, Int savedIp) {
    Int sp = savedSp;
    Int ip = savedIp;
    Int nextSp = ip;
    Int nextIp = sp;
    return nextSp + nextIp;
}

public Int OnTimer(Int ticks) {
    Unsafe {
        ASM("nop");
    }
    return ticks + 1;
}

public Function Start() -> Void {
    Int pages = PageEntry(8192, 3);
    Int switched = SwitchChecksum(100, 200);
    Int ticks = OnTimer(7);
    Int sum = pages + switched + ticks;
    Unsafe {
        if (sum == 8503) {
            ASM("movb $82, %al");
        } else {
            ASM("movb $88, %al");
        }
        ASM("movw $1016, %dx");
        ASM("outb %al, %dx");
        ASM("cli");
        ASM("hlt");
    }
}
