#!/usr/bin/env bash
set -euo pipefail

stage="${1:?Robix stage directory is required}"
root="$(cd "$(dirname "$0")/../.." && pwd)"

install -m 644 "$root/README.md" "$stage/README.md"
install -m 644 "$root/LICENSE" "$stage/LICENSE"
rm -f "$stage/README.txt"

mkdir -p "$stage/Shared/Tools/Llvm"
install -m 644 "$root/Licenses/LLVM-LICENSE" "$stage/Shared/Tools/Llvm/LLVM-LICENSE"

if [[ -d "$stage/Library/Link" ]]; then
    install -m 644 "$root/Licenses/GCC-RUNTIME-LICENSE" "$stage/Library/Link/LICENSE-GCC"
    install -m 644 "$root/Licenses/GLIBC-LICENSE" "$stage/Library/Link/LICENSE-GLIBC"
fi

bytes() {
    wc -c < "$1" | tr -d ' '
}

if (( $(bytes "$stage/README.md") < 2000 )); then
    echo "error: README.md is not the complete readme" >&2
    exit 1
fi
if (( $(bytes "$stage/LICENSE") < 30000 )); then
    echo "error: LICENSE is not the complete Robix license" >&2
    exit 1
fi
if (( $(bytes "$stage/Shared/Tools/Llvm/LLVM-LICENSE") < 14000 )); then
    echo "error: LLVM-LICENSE is not the complete LLVM license" >&2
    exit 1
fi
if ! grep -q 'GNU AFFERO GENERAL PUBLIC LICENSE' "$stage/LICENSE"; then
    echo "error: LICENSE is missing the Affero license text" >&2
    exit 1
fi
if ! grep -q 'LLVM Exceptions to the Apache 2.0 License' "$stage/Shared/Tools/Llvm/LLVM-LICENSE"; then
    echo "error: LLVM-LICENSE is missing the LLVM license text" >&2
    exit 1
fi
if ! grep -q 'Shared/Tools/Llvm/LLVM-LICENSE' "$stage/README.md"; then
    echo "error: README.md does not point at the LLVM license" >&2
    exit 1
fi
