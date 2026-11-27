#!/usr/bin/env bash
# Build every architecture the bootstrap knows.
# The host target and windows-x64 must succeed. Other Linux targets are
# attempted and reported when the cross compiler or sysroot is missing.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
bash "$root/.github/scripts/ensure-bootstrap.sh"
boot="$root/Build/bootstrap/robix-run.app"
failed=0
while read -r name triple; do
    [[ -z "$name" ]] && continue
    echo "bootstrap: target ${name} (${triple})" >&2
    if (
        cd "$root"
        "$boot" Build --emit Robix --emit Robixc --target "$name"
    ); then
        continue
    fi
    case "$name" in
        linux-x64|windows-x64)
            echo "error: ${name} failed" >&2
            failed=1
            ;;
        *)
            echo "warning: ${name} was not built" >&2
            ;;
    esac
done < <("$boot" --targets)
exit "$failed"
