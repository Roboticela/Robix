package Examples.Machine;

public Function Main() -> Void {
    Int cell = 0;
    Unsafe {
        Pointer<Int> device = &cell;
        Volatile.Store(device, 65);
        Print(Volatile.Load(device));
    }
}
