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
| LLVM: `llc`, `lld`, and `libLLVM` | `Shared/Tools/Llvm/LLVM-LICENSE` (Apache License 2.0 with LLVM Exceptions) |
| The C library and the GCC runtime used only to link a program | `Library/Link/LICENSE-GLIBC` and `Library/Link/LICENSE-GCC` |

The Robix license does not cover LLVM. The LLVM license does not cover Robix. An update archive does not replace `Shared/Tools/Llvm` or `Library/Link`, so those licenses stay with the install you already have.

## Install on Linux

Unzip `robix-linux-x64.zip` and add `Bin` to `PATH`.

```bash
unzip robix-linux-x64.zip
export PATH="$PWD/Robix/Bin:$PATH"
Robix run Hello.rx
Robixc SourceFolder Program
```

`Robix` runs a program. Nothing else has to be installed for that. `Robixc` compiles every `.rx` file in a folder into one program. It uses the bundled `llc` and `ld.lld`, then links `Library/Runtime.lib`. Files named `*.test.rx` are tests and are not compiled into the program.

## Install on Windows

Unzip `robix-windows-x64.zip`. The folders match the Linux toolchain. `Robix.exe` and `Robixc.exe` are in `Bin`. `llc.exe` and `ld.lld.exe` are in `Shared\Tools\Llvm\bin`.

Add `Bin` to `PATH` the same way you add any other folder.

## What is in this folder

| Path | Purpose |
| --- | --- |
| `Bin/Robix` | Linux interpreter. |
| `Bin/Robixc` | Linux compiler driver. |
| `Bin/Robix.exe` | Windows interpreter / runner. |
| `Bin/Robixc.exe` | Windows compiler driver. |
| `Packages/Builtin` | Compiled built-in packages. |
| `Packages/Installed` | Place packages you install here. |
| `Shared` | Compiled `.bin` files, grouped by area. UI files are in `Shared/UI`. |
| `Shared/Tools/Compiler` | Linux compiler engine. |
| `Shared/Tools/Llvm` | `llc` and `lld`, with LLVM's own license. |
| `Library/Runtime.lib` | Runtime linked into a compiled program. |
| `Library/Link` | C and C++ link files, with their own licenses. |
| `Runtime/Runtime.o` | Compiled runtime object. |
| `VERSION.txt` | The version of this archive. |
| `LICENSE` | The Robix license. |
| `README.md` | This file. |

## Update an existing install

`Update-<version>-linux-x64.zip` and `Update-<version>-windows-x64.zip` contain the Robix files without LLVM and without `Library/Link`. Unzip the matching archive over the folder that already contains `Robix`. `Shared/Tools/Llvm`, `Shared/Tools/Llvm/LLVM-LICENSE`, and `Library/Link` stay where they are.

```bash
unzip -o Update-0.0.0-linux-x64.zip -d /path/to/parent-of-Robix
```

Use the version from `VERSION.txt` in place of `0.0.0`.

## Editor

`Roboticela.Robix.vsix` is the editor package at the same version. In Visual Studio Code or Cursor, choose **Extensions: Install from VSIX** and select that file. The editor's own readme is inside the package. Its license is the same Robix license as `LICENSE` in this archive.
