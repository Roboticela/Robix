#!/usr/bin/env bash
# Download llvm-mingw and print its bin directory.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
cache="$root/dist/.cache"
mkdir -p "$cache"

linux_tar="$cache/llvm-mingw-linux.tar.xz"
linux_url="https://github.com/mstorsjo/llvm-mingw/releases/download/20261117/llvm-mingw-20261117-ucrt-ubuntu-22.04-x86_64.tar.xz"

size=0
if [[ -f "$linux_tar" ]]; then
    size=$(stat -c%s "$linux_tar" 2>/dev/null || stat -f%z "$linux_tar" 2>/dev/null || echo 0)
fi
if (( size < 80000000 )); then
    curl -L --fail --retry 3 -o "$linux_tar.part" "$linux_url"
    mv "$linux_tar.part" "$linux_tar"
fi
if [[ ! -d "$cache/llvm-mingw-linux" ]]; then
    rm -rf "$cache/llvm-mingw-linux"
    mkdir -p "$cache/llvm-mingw-linux"
    tar -xJf "$linux_tar" -C "$cache/llvm-mingw-linux"
fi

toolchain=$(find "$cache/llvm-mingw-linux" -mindepth 1 -maxdepth 1 -type d -print -quit)
if [[ ! -x "$toolchain/bin/x86_64-w64-mingw32-clang++" ]]; then
    echo "error: llvm-mingw was not found in $cache" >&2
    exit 1
fi
printf '%s\n' "$toolchain/bin"
