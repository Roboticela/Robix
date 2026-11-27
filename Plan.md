# Self-compile

`Robix` and `Robixc` are one compiler and one interpreter, written in Robix. They rebuild themselves from the `.rx` sources. A finished build has no C++ bootstrap and no other language in the compile path. The same compiler emits every Linux and Windows target this project ships.

A program compiles inside Robix:

1. `.rx` is parsed and typechecked.
2. The checked program lowers to RXIR.
3. RXIR is selected to RXASM, Robix’s own instruction text.
4. The Robix assembler turns that text into machine-code bytes and wraps them in an ELF or a PE.

No external assembler is used.

Shipped targets:

- Linux: `linux-x64`, `linux-arm64`, `linux-arm`, `linux-riscv64`, `linux-ppc64le`, `linux-s390x`, `linux-loongarch64`
- Windows: `windows-x64`, `windows-arm64`, `windows-x86`

## Checklist

- [x] Record this plan and the self-host command `Robix self`.
- [x] Compile the compiler sources into one module, twice: once without `Robixc.rx` for the interpreter, and once without `Robix.rx` for the compiler. `Robix self` keeps the 32 compiler, interpreter, and runtime sources.
- [x] Clear the strict checker. Both images report 0 errors. Partial classes such as `Parser` are merged before type checking.
- [x] Lower the module to RXIR and encode every x86-64 opcode it uses. Neither image emits a `; skip` instruction. The text optimizer stays off until it is linear.
- [ ] Give every call a label in the image, including `List`, `Count`, and `At`, and write `Build/Self/Robix.app` and `Build/Self/Robixc.app` as Linux x86-64 ELFs.
- [ ] The runtime those binaries use, including objects, lists, strings, and print, is Robix code lowered to RXASM. It does not call into a C++ runtime.
- [ ] `Build/Self/Robix.app run Examples/HelloRobot.rx` prints `Hello, Robix`.
- [ ] `Build/Self/Robixc.app` compiles `Examples/HelloRobot.rx` into an ELF that prints `Hello, Robix` and `15`.
- [ ] Run `Robix self` with `Build/Self/Robix.app` and confirm the second generation also runs Hello.
- [ ] Emit `windows-x64` the same way, as a PE, and run the Windows Hello check.
- [ ] Teach the assembler to emit RXASM for `linux-arm64`, `linux-arm`, `linux-riscv64`, `linux-ppc64le`, `linux-s390x`, and `linux-loongarch64`, and wrap each one as an ELF.
- [ ] Teach the assembler to emit RXASM for `windows-arm64` and `windows-x86`, and wrap each one as a PE.
- [ ] `Robixc` compiles Hello for each shipped target, and each binary prints `Hello, Robix` and `15`.
- [ ] `Robix self` on each shipped host rebuilds `Robix` and `Robixc` for that host. The second generation on that host runs Hello.
- [ ] Point `build-host.sh` at `Robix self` for every shipped host.
- [ ] Remove `Bootstrap/` and `Bin/RobixBootstrap` once every shipped host can rebuild itself and compile Hello. After that, building Robix or compiling a Robix program does not require C++ or any other language.
