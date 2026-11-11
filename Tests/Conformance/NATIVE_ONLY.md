# Operations the interpreter refuses

These run in a native build. The interpreter stops and names the operation. It does not return a stand-in value.

| Operation | Interpreter |
| :--- | :--- |
| `Port.Read8`, `Port.Read16`, `Port.Write8` | Refuses. Message says the call runs in a native freestanding image |
| `CPU.*` | Refuses with the same message |
| `Timer.Arm`, `Timer.Preempt` | Refuses with the same message |
| `System.Call` | Refuses with the same message |
| `Console.Write` | Refuses with the same message |
| `Memory.Pages`, `Memory.Address`, `Memory.At` | Refuses with the same message |
| `ASM` other than `NOP` | Refuses. `ASM("NOP")` is a no-op on both engines |

`Memory.Allocate` and `Memory.Free` run in the interpreter. `Tests/Conformance/NativeOnly.test.rx` is the executed check for `Port.Write8`.

`Test native` is compiled and run as a Windows executable. A native compile failure fails `robix test` even when the interpreter passed. `Assert.Equal` and `Assert.True` on integers are the assertions that compile. Other `Assert.*` methods stay on the interpreter.
