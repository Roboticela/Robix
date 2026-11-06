# Robix implementation status

This file records what has actually been compiled and run. A row is **Implemented** only when `robixc` accepts it and a program using it has been compiled or executed. **Partial** means the frontend or a document describes it, and the native compiler or the interpreter does not yet carry it through. **Unsupported** means a native or IR build reports an error, or the tool does not exist.

Checked against `Compiler/CodeGen/NativeBackend.cpp`, `Compiler/IR/SSAIR.cpp`, and `Examples/StageB/LibcShape.rbx`. Compiler version string: `robixc version 1.0.0`.

## What `robixc -O3` compiles today

Implemented for top-level functions:

- Packages, imports that resolve inside the compilation, and `Main()` with no parameters.
- `Bool`, fixed-width integers (`Int8`, `Int16`, `Int32`, `Int64`, `UInt8` / `Byte`, `UInt16`, `UInt32`, `UInt64`), `Int`, `Float` / `Double`. Unsigned compares use unsigned predicates. A fitting integer literal may initialize a narrower integer.
- `let`, `var`, `const`, and explicit types.
- `if` / `else`, `while`, numeric `for` (including `..` and `..=`), `break`, `continue`, `return`.
- Integer add, sub, mul, div, rem, comparisons, `and` / `or` / `not`. Release integer math wraps. It is not `nsw`. A positive power-of-two divisor lowers to a shift or mask; a negative dividend uses the toward-zero adjustment instead of `sdiv`.
- Floating-point arithmetic marked `fast` in release builds.
- Direct calls, including overloads distinguished by argument count.
- `extern` functions with the C calling convention. `Examples/StageB/LibcShape.rbx` calls `puts`. Robix functions themselves stay `fastcc` and are not exported for C to call.
- `Print` of booleans, integers, floats, and strings.
- String concatenation. Two visible literals become one global; other concatenations call `rbx_strcat`. `==` and `!=` use `strcmp`.
- Arrays, indexing, and slices (`..` and `..=`), with a bounds trap. Structs, enums, `Tuple`, `Result` (`Success` / `Error`), and `Option` (`Some` / `None`).
- `Unsafe` pointers: `&`, `*`, pointer add and subtract, `Memory.Allocate`, `Memory.Free`.
- Equality-chain `if` lowered to an LLVM `switch` when the chain is long enough.
- Native pipeline: SSA IR, LLVM IR text, `opt -O3` with vertical SLP and horizontal SLP off, a late reassociate pass, then `clang -O3 -march=native`.

Stage B exit run, both engines, checksum `506`:

```text
robix run Examples/StageB/LibcShape.rbx
robixc -O3 Examples/StageB/LibcShape.rbx -o Build/StageB.exe
```

Both printed `ok`, `Hello`, and `506`.

Stage C exit run, both engines, `Examples/StageC/Machine.rbx`:

```text
robix run Examples/StageC/Machine.rbx
robixc -O3 Examples/StageC/Machine.rbx -o Build/StageC.exe
```

Both printed `150` and then `40`. That program has a `Motor` class with a private field, a constructor, methods, and a destructor; `Drive`, `Walker`, and `Runner` with `override`; a `Box<Int>` instantiation; and `Leaf`, which adds one and returns. The `40` is the destructor printing the motor speed after `Main` prints the total. `Examples/StageC/PrivateField.rbx` is rejected with `R2008` because `Main` reads the private field.

Class objects are a pointer. Word 0 is the vtable. Fields follow, in alphabetical order, one word each. A call whose `new` is visible in the same function is direct. `MakeRunner` returns a `Runner` as a `Walker`, and that call stays virtual: the unoptimized IR has `call fastcc i64 %t7(ptr %v28)`. The interpreter walks `Runner` then `Walker` for the same call. `Box<Int>` is lowered as its own functions (`Box$Int.Get`, `Box$Int.Set`), not as the open template. `Leaf` is `load`, `add`, `ret`. It does not call `malloc`. There is no collector. Class locals with a destructor are destroyed, then `free`d, at the end of the scope. Reference counting is not implemented.

One interface's methods occupy the front of the vtable, sorted by name. A class with two interfaces is not a supported slot layout. C still cannot call a Robix function.

## Partial

| Feature | What exists | What does not |
| :--- | :--- | :--- |
| Shared frontend | Lexer, parser, type check, SSA IR for the scalar subset | The interpreter and the native compiler are not proven equivalent by a conformance suite |
| Bytecode interpreter | `Interpreter/VM/BytecodeVM.hpp` and a bytecode module | Not the default proof of language behavior |
| Strings | Literals, concatenation, `==` / `!=`, and `Print` | `.ToString()` is still a native error. Ordering comparisons are not implemented |
| Ranges | Numeric range loops, array index, and slices | `for x in collection` is still an IR error |
| `match`, `try`, `throw` | Parsed | SSA IR rejects them |
| Lambdas and object literals | Parsed | SSA IR rejects them |
| C ABI | Robix calls an `extern` C function | C cannot call a Robix function; there is no unmangled `ccc` export |
| Build profiles | `-O0` debug and `-O2`/`-O3`/`--release` | `performance`, `realtime`, `embedded`, `safety`, and `size` are not separate compiler modes |
| Targets | Host Windows x64 via the bundled Clang | No `*-unknown-none` freestanding link, no cross-built images |
| Benchmarks | Local kernels with C and C++ twins, timed by `Benchmarks/RunBenchmarks.ps1` | Not SPEC, DaCapo, NPB, or the Benchmarks Game. See `Benchmarks/README.md` |
| Specification | `Specification/LanguageSpecification.md` and grammar files | The documents describe a larger language than this table |

## Unsupported on the native backend

`robixc` reports `N9001` or `I4010` for these. They are not native-complete.

- Global constants.
- `defer` and `using`.
- Garbage collection. There is no collector in the native runtime. `Memory.Allocate` / `Memory.Free` are explicit `malloc` / `free`, not a collector.
- OS intrinsics other than `Memory.Allocate` and `Memory.Free`.
- GPIO, PWM, and any other hardware API. A call does not read a pin.
- Packages as a shippable registry. `.rbxproj` / `.rbxp` tooling is not the native compile path.
- RobixUI (`.rbxu`), RobixBuild (`.rbxb`), and Robix Network.
- LSP over the wire, a VS Code extension, and a debugger attached to the native binary.

## Commands that exist

| Command | Status |
| :--- | :--- |
| `robixc File.rbx -o App -O3` | Implemented |
| `robixc --version`, `robixc --help`, `robixc --emit-llvm`, `robixc --keep-ir` | Implemented |
| `robix run File.rbx` | Interpreter. Recorded on `Examples/StageB/LibcShape.rbx` (checksum `506`) and `Examples/StageC/Machine.rbx` (`150`, then `40`) |
| `robix repl`, `robix test`, `robix new` | Not recorded as done |

## How to update this file

Change a row only after a command was run. Record the command and the result under the row. Do not mark a feature Implemented because a specification section or an example file mentions it.
