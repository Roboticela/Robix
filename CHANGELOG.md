# Changelog

All notable changes to Robix are recorded here. The version name matches the string printed by `robix version` and `robixc --version`.

The detailed run log is [Documentation/Status.md](Documentation/Status.md). A line appears there only after a command was actually run.

## 1.0.0

The first public description of the toolchain in this repository.

- Shared C++20 frontend: lexer, parser, type checker, and SSA IR.
- `robix` interprets `.rbx` programs. `robixc` compiles them through LLVM.
- Hosted Windows executables, static libraries, shared libraries, object files, and `.rbxlib` packages.
- Freestanding images for Kernel and Firmware projects, booted under QEMU on x86-64, AArch64, ARM, and RISC-V.
- RobixUI documents on Windows and on the web, with type-checked event bindings.
- Project categories: Application (Console, Window, Network), Canvas, and Bedrock.
- Formatter, linter, checker, documentation generator, language server, REPL, and the Visual Studio Code extension `roboticela.robix`.
- Licensed under the GNU Affero General Public License v3.0. Copyright (C) 2026 Roboticela.

## Unreleased

Changes land here before the next version number, with the command that demonstrated them.
