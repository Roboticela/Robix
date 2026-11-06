package Examples.StageF;

public Int PageEntry(Int phys, Int flags) {
    return (phys & ~0xFFF) | flags;
}

public Function Main() -> Void {
    Int entry = PageEntry(0x2000, 0x3);
    Int page = 1 << 12;
    Int low = 0xFF & ~0x0F;
    Int mixed = (0xF0 ^ 0x0F) >> 1;
    Int negative = -8 >> 2;
    Print(entry);
    Print(page);
    Print(low);
    Print(mixed);
    Print(negative);
}
