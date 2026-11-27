# Self-compile

`Robixc` must rebuild `Robix` and `Robixc` from the `.rx` sources. The C++ bootstrap stays stage 0 until the rebuilt compiler can run `HelloRobot.rx` and rebuild the same sources again.

The current project build typechecks each file alone and then wraps that file as its own executable. The compiler is one program spread across many files, including partial classes, so that build cannot replace the bootstrap.

## Checklist

- [x] Record this plan and the self-host command `Robix self`.
- [x] Compile the compiler sources into one module, twice: once without `Robixc.rx` for the interpreter, and once without `Robix.rx` for the compiler. `Robix self` keeps the 32 compiler, interpreter, and runtime sources. It does not parse `Examples/AdvancedTypes.rx`, which is an ELF saved with a `.rx` name.
- [ ] Clear the strict checker. The merged module currently reports 762 errors and 715 warnings. The errors that block one file at a time are RX173 (a package has no Description), RX110 (nullable member access), and RX103 (a value does not match the destination type). Lowering starts only after this count is zero, because lowering the rejected module does not finish.
- [ ] Merge partial classes, such as `Parser`, before type checking and lowering. `Robix self` now folds later parts of a class into the first part.
- [ ] Make that module pass the strict checker. Fix checker bugs and source errors until `Robix self` reports zero errors.
- [ ] Lower the module to RXIR and encode every opcode it uses. A `; skip` line means the assembler is not finished.
- [ ] Write `Build/Self/Robix.app` and `Build/Self/Robixc.app` as one Linux x86-64 ELF each.
- [ ] `Build/Self/Robix.app run Examples/HelloRobot.rx` prints `Hello, Robix`.
- [ ] `Build/Self/Robixc.app` compiles `Examples/HelloRobot.rx` into an ELF that prints `Hello, Robix` and `15`.
- [ ] Run `Robix self` with `Build/Self/Robix.app` and confirm the second generation also runs Hello.
- [ ] Emit the Windows x64 PE the same way.
- [ ] Leave other architectures on the C++ bootstrap until the assembler can emit them.
- [ ] Point `build-host.sh` at `Robix self` only after the second generation succeeds.

## Out of scope for the first self-host

The released `Bin/RobixBootstrap` binary and `Bootstrap/main.cpp` stay until the checklist above is done. Deleting them before that removes the only builder that can produce `Robix` and `Robixc`.
