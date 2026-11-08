<p align="center">
  <img src="Assets/Brand/Robix.svg" alt="Robix — The Language of Intelligent Machines" width="720">
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-0E7AFF" alt="License AGPL-3.0"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20-0B1220" alt="C++20">
  <img src="https://img.shields.io/badge/version-1.0.0-1C1C1E" alt="Version 1.0.0">
  <img src="https://img.shields.io/badge/source-.rbx-3B9EFF" alt="Source extension .rbx">
  <img src="https://img.shields.io/badge/UI-.rbxu-7CC4FF" alt="UI extension .rbxu">
</p>

<p align="center">
  <b>High performance. Complete control. Intelligent machines.</b><br>
  A statically typed, object-oriented systems language, a native compiler, a bytecode interpreter, and a declarative UI — one ecosystem by <b>Roboticela</b>.
</p>

<p align="center">
  <a href="#-start-here">Start</a> ·
  <a href="#-two-engines">Engines</a> ·
  <a href="#-projects">Projects</a> ·
  <a href="#-the-language">Language</a> ·
  <a href="#-robixui">RobixUI</a> ·
  <a href="#-toolchain">Toolchain</a> ·
  <a href="#-build">Build</a> ·
  <a href="#-documentation">Docs</a> ·
  <a href="#-community">Community</a>
</p>

---

Robix is for programs that talk to people, programs that talk to machines, and the machine image itself. You write one language. `robix run` executes it while you are building. `robixc` compiles it to native code when you are shipping.

`Documentation/Status.md` is the record of commands that have actually been run. This page describes the language and the tools in the repository.

## ✨ Start here

```robix
package Examples.Hello;

public Int Add(Int a, Int b) {
    return a + b;
}

public Function Main() -> Void {
    Print("Hello, Robix");
    Print(Add(20, 22));
}
```

```bash
robix run Examples/StageD/Hello.rbx
robixc -O3 Examples/StageD/Hello.rbx -o Build/Hello.exe
```

Both print:

```text
Hello, Robix
42
```

That sample lives at `Examples/StageD/Hello.rbx`. The walkthrough is `Documentation/Guide.md`.

## ⚙️ Two engines

One frontend. Two ways to run the same program.

| | 🧪 Development | 🚀 Production |
| --- | --- | --- |
| Command | `robix run Main.rbx` | `robixc -O3 Main.rbx -o App` |
| Engine | Bytecode interpreter | LLVM, then the system linker |
| Goal | Fast feedback, REPL, UI preview | A native executable, library, or machine image |
| Default | Interpreter | Native compiler |

`robix run File.rbx --engine native` asks for a native run. A native compile that fails is not silently replaced by the interpreter.

The compiler version string is `robix version 1.0.0`.

## 🗂️ Projects

`robix new` asks for a category, then a kind. A blank answer is rejected. Passing both on the command line skips the questions.

| Category | What it is | Kinds |
| --- | --- | --- |
| 🖥️ **Application** | A program a person runs | **Console** · **Window** · **Network** |
| 🎨 **Canvas** | An empty folder you shape file by file | **Empty** |
| 🪨 **Bedrock** | The machine itself. A freestanding image | **Kernel** · **OS** · **BIOS** · **Driver** · **Bootloader** · **Firmware** |

```bash
robix new Spot Window
robix new Spot Bedrock Kernel
robix new Pad Canvas
```

**Console** compiles for `windows`, `linux`, `macos`, `android`, and `ios`. Web is refused.

**Window** and **Network** use those platforms and `web`, when the project has a `.rbxu` screen. A missing screen is an error.

**Canvas** is classified from the file. The tool prints a warning naming that choice, then applies the same web rule. `Print`, `Input`, or `Args` is a console program. `Network` or `SendText` is a network program. A matching `.rbxu` is a window.

**Bedrock** builds a machine image. It does not target an operating system. `robix run` of Bedrock on a platform stops with an error. `robixc --image` or the `.rbxb` build file produces the image.

On this Windows toolchain, `windows` and `web` are the platforms that are actually interpreted, and `windows` is the platform that is actually compiled. Other platform names are accepted and then refused with a direct error. A QEMU boot of a freestanding image is recorded in `Documentation/Status.md`. It is not a claim that the image ran on hardware.

Machine code (`Cpu`, `Port`, `Console.Write`, `Timer`, `System.Call`, `Memory.Pages`, `Memory.Address`, `Memory.At`) aimed at a platform stops before execution. Platform code (`Print`, `Input`, `File`, `Args`, `Env`, `Network`, `Thread`, and the hosted hardware calls) aimed at an image stops the same way.

## 🧠 The language

Robix is object-oriented first, and it still lets you write a function and call it.

```robix
package Robotics;

public class Robot {
    private String robotName;
    private Int batteryLevel;

    public Robot(String name) {
        this.robotName = name;
        this.batteryLevel = 100;
    }

    public Void Start() {
        Print("Robot started");
    }
}
```

| | |
| --- | --- |
| 📝 **Source** | `.rbx` programs, `.rbxh` interfaces, `.rbxproj` projects, `.rbxb` build files |
| 🔤 **Names** | `PascalCase` types and functions, `camelCase` variables, `UPPER_SNAKE_CASE` constants, acronyms stay uppercase (`HTTPClient`, `USBDevice`) |
| 🔒 **Types** | `Bool`, fixed-width integers, `Int`, `UInt`, `Float`, `Double`, `String`, `Void`, arrays, structs, enums, classes, interfaces, `Option`, `Result` |
| 🧩 **Programs** | Classes, interfaces, inheritance, `override`, generics, top-level functions, `let` / `var` / `const` |
| 🔁 **Control** | `if`, `match`, `for`, `while`, `break`, `continue`, `return`, `try` / `catch`, `defer`, `using` |
| 🧮 **Memory** | Ordinary objects are freed at the end of the scope. `Unsafe` is where `&`, `*`, `Memory.Allocate`, and `Memory.Free` live. There is no garbage collector |
| 🧷 **Low level** | Pointers, `extern` C functions, `ASM`, `Volatile.Load` / `Volatile.Store`, freestanding `Start` |
| 📦 **Packages** | `import`, lockfiles, `robix add` from a local file or an `http` URL checked with an HMAC-SHA256 publisher key |

The formal grammar is `Specification/RobixGrammar.ebnf`. The feature catalog is `Documentation/LanguageFeatures.md`.

A class that has been compiled and run is `Examples/StageC/Machine.rbx`: a private field, a constructor, methods, a destructor, `override`, and a `Box<Int>` instantiation. Both engines printed `150` and then `40`.

## 🪟 RobixUI

RobixUI is the declarative UI language. Documents are `.rbxu` and stay XML-compatible.

```xml
<RobixUI Version="1.0">
  <Window Name="RobotDashboard" Title="Roboticela Robot Control" Width="1000" Height="700">
    <Column Spacing="16">
      <Text Value="Robot Dashboard" FontSize="28" FontWeight="Bold"/>
      <Row Spacing="12">
        <Button Name="StartButton" Text="Start Robot" OnClick="StartRobot"/>
        <Button Name="StopButton" Text="Stop Robot" OnClick="StopRobot"/>
      </Row>
    </Column>
  </Window>
</RobixUI>
```

`OnClick` is type-checked against a Robix method. `robix ui validate` rejects a handler whose signature does not match.

Screens and boards render on **Windows** and on the **web**. The Windows host paints a framebuffer. The web host writes a page. Linux, macOS, Android, and iOS do not have a UI backend in this tree.

The default typeface is [Inter](https://github.com/rsms/inter), under the SIL Open Font License. See `Assets/Fonts/Inter/LICENSE.txt`.

## 🛠️ Toolchain

| Command | What it does |
| --- | --- |
| `robix run File.rbx` | Interpret a program |
| `robix run --engine native` | Run through the native compiler |
| `robix repl` | Interactive line, same parser and type checker |
| `robix new` | Create a project |
| `robix build` | Build the project, incrementally when nothing changed |
| `robix test` | Run `*.test.rbx` on the interpreter and the native compiler |
| `robix check` | Type-check |
| `robix format` / `robix lint` | Format and style |
| `robix doc` | Write Markdown from `Explain` blocks |
| `robix debug` | Source lines. `--attach` uses the Windows debug API |
| `robix lsp` | Language server. The editor extension is `Editors/vscode` |
| `robix ui validate` | Check a `.rbxu` document and its event bindings |
| `robix ui window` / `robix ui web` | Show a screen on Windows or write the web page |
| `robixc -O3 File.rbx -o App` | Native executable |
| `robixc --emit-llvm` | Keep the LLVM IR |
| `robixc --image` | Freestanding machine image |
| `robixc --profile performance` | Also `release`, `debug`, `realtime`, `safety`, `size`, `embedded` |

Profiles select compiler flags. `performance` is `-O3 -funroll-loops`. `realtime` is `-O2 -fno-strict-aliasing`. `safety` is `-O0`. `size` is `-Oz`. `embedded` is a freestanding object.

Native outputs that have been linked on this machine: an executable, a static library, a shared library, an object file, and a `.rbxlib` package. A kernel image has booted under QEMU on x86-64, AArch64, ARM, and RISC-V and written a serial byte. That is QEMU, not a board on a desk.

## 🏗️ Build

The toolchain is C++20. On Windows the build script is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/BuildCompiler.ps1
```

It looks for Clang 18, then `clang++` on `PATH`. The binaries land in `Build/Bin`:

```text
Build/Bin/robix.exe
Build/Bin/robixc.exe
```

`CMakeLists.txt` builds the C++ libraries and the stage tests. The script above is what produces `robix` and `robixc`.

```text
Robix/
├── Compiler/        lexer, parser, types, IR, LLVM
├── Interpreter/     bytecode virtual machine
├── Runtime/         native runtime
├── StdLib/          Robix standard library
├── RobixUI/         documents, layout, Windows, web
├── Tools/           robix, robixc, formatter, linter, LSP
├── Specification/   grammar and language specification
├── Documentation/   guide, feature catalog, status
├── Examples/        programs that have been run
├── Tests/           conformance tests
└── Editors/vscode   syntax and language client
```

## 📚 Documentation

| | |
| --- | --- |
| [Guide](Documentation/Guide.md) | The programs to run first |
| [Language features](Documentation/LanguageFeatures.md) | Syntax, types, and library surface |
| [Status](Documentation/Status.md) | What has actually been compiled and run |
| [Language specification](Specification/LanguageSpecification.md) | Semantic rules |
| [Grammar](Specification/RobixGrammar.ebnf) | Lexical and syntactic grammar |
| [RobixUI grammar](Specification/RobixUIGrammar.ebnf) | UI document grammar |
| [ABI](Specification/ABISpecification.md) | Calling and object layout |

## 🤝 Community

| | |
| --- | --- |
| [Contributing](CONTRIBUTING.md) | Build, test, and the rule for status rows |
| [Security](SECURITY.md) | How to report a vulnerability |
| [Code of conduct](CODE_OF_CONDUCT.md) | How we work together |
| [Changelog](CHANGELOG.md) | Version history |
| [License](LICENSE) | GNU Affero General Public License v3 |

## ⚖️ License

Copyright (C) 2026 Roboticela.

Robix and RobixUI are free software under the **GNU Affero General Public License v3.0**. You can run, study, change, and share this work. If you modify it and let others use that modification over a network, they receive the source of your version. The full terms are in [LICENSE](LICENSE).

The Inter font files under `Assets/Fonts/Inter` stay under the SIL Open Font License 1.1.
