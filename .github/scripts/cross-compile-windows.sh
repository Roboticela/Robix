#!/usr/bin/env bash
# Cross-compile the interpreter, the compiler, and the C++ bootstrap for Windows.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
tools="$root/dist/windows-tools"
mkdir -p "$tools"

mingw_bin="$(bash "$root/.github/scripts/ensure-mingw.sh")"
cxx="$mingw_bin/x86_64-w64-mingw32-clang++"
if [[ ! -x "$cxx" ]]; then
    echo "error: the mingw compiler was not found at $cxx" >&2
    exit 1
fi

bash "$root/.github/scripts/ensure-bootstrap.sh"
boot="$root/Build/bootstrap/robix-run.app"
if [[ ! -x "$boot" ]]; then
    echo "error: Build/bootstrap/robix-run.app is missing" >&2
    exit 1
fi

export PATH="$mingw_bin:${PATH}"
toolchain="$(cd "$mingw_bin/.." && pwd)"
export LD_LIBRARY_PATH="$toolchain/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$cxx" --version >/dev/null

(
    cd "$root"
    "$boot" Build --emit Robix --emit Robixc --target windows-x64
)
if [[ ! -f "$tools/Robix.exe" || ! -f "$tools/Compiler.exe" ]]; then
    echo "error: windows-x64 did not write Robix.exe and Compiler.exe" >&2
    exit 1
fi

"$cxx" -std=c++20 -O2 -static -o "$tools/RobixBootstrap.exe" "$root/Bootstrap/main.cpp"
echo "cross-compiled Windows x64 tools" >&2
