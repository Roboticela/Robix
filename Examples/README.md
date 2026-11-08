# Examples

Programs in this folder are grouped by what they show. Each file is a real Robix program, a RobixUI document, or the C file that calls one.

| Folder | What you will find |
| --- | --- |
| [GettingStarted](GettingStarted) | Hello, a point, a loop, division, input, and a type mismatch |
| [Classes](Classes) | Classes, inheritance, and a private field |
| [Interop](Interop) | A hosted program that calls the C function `puts` |
| [Language](Language) | `match`, `try`, `defer`, `using`, a lambda, two interfaces, and a large program |
| [Packages](Packages/Answer) | A project with a locked local package and required tests |
| [UserInterface](UserInterface/Dashboard) | A window, its Robix handlers, and a document that fails validation |
| [Network](Network) | A local client and server, and a project whose secret is shared on purpose |
| [DeveloperTools](DeveloperTools) | Format, lint, check, documentation, the debugger, and the language server |
| [NativeBuild](NativeBuild) | An executable, libraries, an object file, a package, and a Linux build file |
| [Machine](Machine) | Serial output, a kernel image, firmware, a disk read, boards, and inline assembly |

```text
robix run Examples/GettingStarted/Hello.rbx
robixc -O3 Examples/GettingStarted/Hello.rbx -o Build/Hello.exe
```

`Documentation/Status.md` records the commands that were run before these folders were renamed. The sources are the same files.

| Earlier path | Current path |
| --- | --- |
| `Examples/StageB/LibcShape.rbx` | `Examples/Interop/HostedCore.rbx` |
| `Examples/StageC/Machine.rbx` | `Examples/Classes/RobotClasses.rbx` |
| `Examples/StageD/` | `Examples/GettingStarted/` |
| `Examples/StageF/` | `Examples/Machine/` |
| `Examples/StageF/BadMnemonic.rbx` | `Examples/Machine/RejectedInstruction.rbx` |
| `Examples/StageI/` | `Examples/Packages/Answer/` |
| `Examples/StageJ/` | `Examples/UserInterface/Dashboard/` |
| `Examples/StageK/` | `Examples/Network/LocalCall/` |
| `Examples/StageK-bad/` | `Examples/Network/SharedSecret/` |
| `Examples/StageL/` | `Examples/DeveloperTools/` |
| `Examples/StageM/` | `Examples/Language/` |
| `Examples/StageN/` | `Examples/NativeBuild/` |
