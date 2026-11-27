#!/usr/bin/env bash
# Rebuild the interpreter and the compiler with the C++ bootstrap.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
bash "$root/.github/scripts/ensure-bootstrap.sh"
boot="$root/Build/bootstrap/robix-run.app"
(
    cd "$root"
    "$boot" Build --emit Robix --emit Robixc --target host
)
if [[ ! -x "$root/Build/Compiler/Robix.app" || ! -x "$root/Build/Compiler/Robixc.app" ]]; then
    echo "error: the host build did not write Robix.app and Robixc.app" >&2
    exit 1
fi
echo "recompiled the Linux interpreter and compiler" >&2
