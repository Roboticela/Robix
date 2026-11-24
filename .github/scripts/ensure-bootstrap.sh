#!/usr/bin/env bash
# The bootstrap is the compiler that builds this tree once.
# The GitHub release is not that compiler, so this script never downloads it.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
dest="$root/Build/bootstrap/robix-run.app"

if [[ -x "$dest" ]]; then
    exit 0
fi

echo "error: $dest is missing. Compile with that bootstrap before packaging. The latest GitHub release is not the bootstrap." >&2
exit 1
