# Open a Robix file

A `.rx` file is a program. A `.rxp` file is a project. The status bar shows the configuration, the target, and Debug or Release.

# Build

Ctrl+Shift+B runs the Robix build task. **Robix: Configure** writes `Build/compile_commands.json`.

# Debug

F5 starts debugging. Ctrl+F5 runs without debugging. F9 sets a breakpoint. The session uses gdb when the toolchain has produced a binary and gdb is on PATH.

# Test

The Testing view lists every `Test` in `*.test.rx`. Alt+O switches a source file and its companion test.
