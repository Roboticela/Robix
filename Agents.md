# Toolchain layout

A Robix toolchain directory has these folders and no update archive.

## Bin

`Bin` contains executable files. `Robix` and `Robixc` are required. On Windows those files are `Robix.exe` and `Robixc.exe`. They are native programs, not shell scripts. Other executables, such as `RobixBootstrap`, may sit beside them.

## Packages

`Packages/Builtin` contains every built-in package, one folder per package. The folder name is the package name, for example `Robix.Core`. `Packages/Installed` is where a person adds packages later.

A package folder is flat. It holds the source, the tests, and the compiled unit that a program can link:

- `Robix.Core.rx`
- `Robix.Core.tests.rx`
- `Robix.Core.o` on Linux, or `Robix.Core.dll` on Windows

## Shared

`Shared` holds files that exist only because of the operating system. A Windows toolchain records `kernel32` there. A Linux toolchain records the C and C++ link licenses there.

## Documentation

`Documentation` is reserved. Leave it empty until there are documents to put in it.

## Library

Every compiler source file compiles to its own unit. `Library` contains those units and not the source. `Lexer.rx` becomes `Library/Lexer.o` on Linux and `Library/Lexer.dll` on Windows. The same rule applies to every other compiler file.
