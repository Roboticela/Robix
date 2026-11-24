#!/usr/bin/env bash
# Copy the committed Linux bootstrap into Build/bootstrap.
# The GitHub release is not this compiler.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
source="$root/.github/bootstrap/robix-run.app"
dest="$root/Build/bootstrap/robix-run.app"

if [[ ! -f "$source" ]]; then
    echo "error: $source is missing" >&2
    exit 1
fi

mkdir -p "$root/Build/bootstrap"
cp -a "$source" "$dest"
chmod 755 "$dest"
