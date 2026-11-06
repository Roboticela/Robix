package Examples.StageF;

public Int PageEntry(Int phys, Int flags) {
    return (phys & ~0xFFF) | flags;
}
