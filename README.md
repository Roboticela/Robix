# Robix

Robix is Roboticela's language for programs that should feel as direct as C and as clear as a page you can read aloud. Names you invent stay as you wrote them. Keywords are PascalCase. Every value has a type. A function says what it receives, what it does, and what it returns before the body begins.

You can run a file at once with the interpreter, or compile a folder of source files into one native program.

```robix
Package Robotics.Hello;

Import Robix.Core;

Public Function Main(Args<String> args) -> Int
Explain {
    Description: "Greet the operator and total the numbers from 1 through 5.";
    Input: args = "Command-line arguments";
    Processing: "Add the inclusive range and print the greeting and total.";
    Output: "Zero when the greeting has been printed.";
} {
    Var total = 0;
    For index In 1..=5 {
        total = total + index;
    }
    Print("Hello, Robix");
    Print(total);
    Return 0;
}
```

A `.rx` file is a program or a library. A `.rxp` file describes a project. A `.rxu` file is a user interface. A `.rx3d` file is a 3D scene. A `Test` sits beside the function it checks.

## Version

The version of this archive is in `VERSION.txt`. It starts at 0.0.0 and increases by 0.0.1 on each commit of the Robix source.

## License

Three licenses travel with this archive, and they stay separate.

| What | License file |
| --- | --- |
| Robix itself: the interpreter, the compiler, the runtime, the packages, and the editor | `LICENSE` (GNU Affero General Public License, version 3) |
| LLVM | `Shared/LLVM-LICENSE` (Apache License 2.0 with LLVM Exceptions) |
| The C library and the GCC runtime used only to link a Linux program | `Shared/LICENSE-GLIBC` and `Shared/LICENSE-GCC` |

The Robix license does not cover LLVM. The LLVM license does not cover Robix.

## Install on Linux

Unzip the archive for this machine and add `Bin` to `PATH`. The Linux archives are `robix-linux-x64.zip`, `robix-linux-arm64.zip`, `robix-linux-arm.zip`, `robix-linux-riscv64.zip`, `robix-linux-ppc64le.zip`, `robix-linux-s390x.zip`, and `robix-linux-loongarch64.zip`.

```bash
unzip robix-linux-x64.zip
export PATH="$PWD/Robix/Bin:$PATH"
Robix run Hello.rx
Robixc SourceFolder Program
```

`Robix` runs a program. `Robixc` compiles a program. Both are executable files in `Bin`.

## Install on Windows

Unzip `robix-windows-x64.zip`, `robix-windows-arm64.zip`, or `robix-windows-x86.zip`. The folders match the Linux toolchain. `Robix.exe` and `Robixc.exe` are executable files in `Bin`.

Add `Bin` to `PATH` the same way you add any other folder.

## What is in this folder

| Path | Purpose |
| --- | --- |
| `Bin/Robix` | Interpreter. On Windows the file is `Robix.exe`. |
| `Bin/Robixc` | Compiler. On Windows the file is `Robixc.exe`. |
| `Packages/Builtin` | One folder per built-in package, such as `Robix.Core`. |
| `Packages/Builtin/Robix.Core` | `Robix.Core.rx`, `Robix.Core.tests.rx`, and `Robix.Core.o` or `Robix.Core.dll`. |
| `Packages/Installed` | Place packages you install here. |
| `Shared` | Files that belong to this operating system, such as `kernel32` on Windows. |
| `Library` | One compiled unit per compiler source file, such as `Lexer.o` or `Lexer.dll`. |
| `Documentation` | Reserved for documentation. |
| `VERSION.txt` | The version of this archive. |
| `LICENSE` | The Robix license. |
| `README.md` | This file. |

## Editor

`Roboticela.Robix.vsix` is the editor package at the same version. In Visual Studio Code or Cursor, choose **Extensions: Install from VSIX** and select that file. The editor's own readme is inside the package. Its license is the same Robix license as `LICENSE` in this archive.
