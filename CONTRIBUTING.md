# Contributing to Robix

Thank you for working on Robix. This repository is the language, the compiler, the interpreter, RobixUI, and the tools around them. A change should leave the next person able to build it, run it, and see what the run actually did.

## 🌿 Before you start

- Read [README.md](README.md) and [Documentation/Guide.md](Documentation/Guide.md).
- Read [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
- Look at [Documentation/Status.md](Documentation/Status.md) before you describe a feature as working.

By sending a contribution you agree that it is licensed under the [GNU Affero General Public License v3.0](LICENSE), the same license as the rest of this repository.

## 🛠️ Build

The toolchain is C++20. On Windows:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/BuildCompiler.ps1
```

The script uses Clang 18 when it is installed at the path in the script, and `clang++` on `PATH` otherwise. Binaries are written to `Build/Bin/robix.exe` and `Build/Bin/robixc.exe`.

```text
Build/Bin/robix.exe version
Build/Bin/robixc.exe --version
```

`CMakeLists.txt` builds the C++ libraries and the stage tests. The script above is what produces the two programs.

## ✅ Check your change

Run the program that your change affects, on the engine that your change affects.

```powershell
Build\Bin\robix.exe run Examples\GettingStarted\Hello.rbx
Build\Bin\robixc.exe -O3 Examples\GettingStarted\Hello.rbx -o Build\Hello.exe
Build\Bin\robix.exe test Tests\Conformance
```

A compiler change that touches types or code generation should also run a program that exercises that path, and the same program through `robix run` when both engines are supposed to agree.

Record the command and the output you saw. A feature is implemented when a program using it has been compiled or executed. A specification paragraph, a new syntax example, or a function that returns a stand-in value is not a passing test.

## ✏️ How to change the language

- Keep one grammar. The interpreter and `robixc` share the lexer, parser, name resolution, and type checker.
- Update `Specification/RobixGrammar.ebnf` or `Specification/RobixUIGrammar.ebnf` when the syntax changes, and `Documentation/LanguageFeatures.md` when the public surface changes.
- Diagnostics need a source location and a stable code such as `R1001`.
- Unsafe memory, pointers, and inline assembly stay inside `Unsafe`.
- Hardware that is not present on the machine must say so. A Windows run must not invent a GPIO level, a motor speed, or a board boot.
- Platform names this machine cannot build are refused with a direct error. Web is refused for a console program. A window or a network program needs a `.rbxu` screen before it compiles for web.
- Bedrock builds a machine image. It does not target an operating system.

## 🎨 Style

Robix source follows the official names:

- `PascalCase` for types, functions, methods, and packages.
- `camelCase` for variables, fields, and parameters.
- `UPPER_SNAKE_CASE` for constants.
- Acronyms stay uppercase: `HTTPClient`, `USBDevice`.

C++ in this repository is C++20, RAII, const where the value does not change, and a warning-clean build. Match the file you are editing.

`robix format` and `robix lint --strict` are the style tools for `.rbx` files.

## 🧪 Tests

| Kind | Where |
| --- | --- |
| Conformance | `Tests/Conformance` |
| Standard library | `StdLib/Robix/*.test.rbx` |
| A new example | `Examples/`, with the command in the pull request |

`robix test` runs the interpreter and the native compiler. When a feature can only run in a freestanding image, say so in the test note. `Tests/Conformance/NATIVE_ONLY.md` is the list of those cases. Do not mark a native-only program as passing on the interpreter.

## 📜 Status

`Documentation/Status.md` is a historical run log.

- Add or change a row only after the command was run.
- Write the command and the result.
- Leave a feature as partial or unsupported when the run does not carry it through.
- Do not rewrite old rows to match a newer design unless you re-ran them.

## 💬 Pull requests

Use the pull request template. Include:

1. What changed and why.
2. The commands you ran and what they printed.
3. What you did not run.

Small, focused changes are easier to review. A language change, a runtime change, and a documentation change can live together when they are one feature.

## 🔒 Security

Report vulnerabilities through the process in [SECURITY.md](SECURITY.md). A public issue is the wrong place for a crash that can be used against someone else.
