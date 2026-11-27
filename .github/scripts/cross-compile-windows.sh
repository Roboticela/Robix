#!/usr/bin/env bash
# Cross-compile the interpreter, the compiler, and the C++ bootstrap for Windows.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
cache="$root/dist/.cache"
tools="$root/dist/windows-tools"
mkdir -p "$cache" "$tools"

linux_tar="$cache/llvm-mingw-linux.tar.xz"
linux_url="https://github.com/mstorsjo/llvm-mingw/releases/download/20261117/llvm-mingw-20261117-ucrt-ubuntu-22.04-x86_64.tar.xz"

fetch() {
    local url="$1"
    local file="$2"
    local minimum="$3"
    local size=0
    if [[ -f "$file" ]]; then
        size=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file" 2>/dev/null || wc -c < "$file" || echo 0)
    fi
    if (( size < minimum )); then
        curl -L --fail --retry 3 -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
}

fetch "$linux_url" "$linux_tar" 80000000
if [[ ! -d "$cache/llvm-mingw-linux" ]]; then
    rm -rf "$cache/llvm-mingw-linux"
    mkdir -p "$cache/llvm-mingw-linux"
    tar -xJf "$linux_tar" -C "$cache/llvm-mingw-linux"
fi

toolchain=$(find "$cache/llvm-mingw-linux" -mindepth 1 -maxdepth 1 -type d -print -quit)
cxx="$toolchain/bin/x86_64-w64-mingw32-clang++"
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

export PATH="$toolchain/bin:${PATH}"
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

if [[ -x "$toolchain/bin/aarch64-w64-mingw32-clang++" ]]; then
    (
        cd "$root"
        "$boot" Build --emit Robix --emit Robixc --target windows-arm64
    )
fi

"$cxx" -std=c++20 -O2 -static -o "$tools/RobixBootstrap.exe" "$root/Bootstrap/main.cpp"
echo "cross-compiled Windows x64 tools" >&2
