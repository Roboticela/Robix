package Examples.StageF;

public Int PageEntry(Int phys, Int flags) {
    Int aligned = phys / 4096;
    return aligned * 4096 + flags;
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

public Function Main() -> Void {
    Int pages = PageEntry(8192, 3);
    Int switched = SwitchChecksum(100, 200);
    Int ticks = OnTimer(7);
    Print(pages + switched + ticks);
}
