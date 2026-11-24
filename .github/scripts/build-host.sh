#!/usr/bin/env bash
# Recompile the interpreter and the compiler engine for this source tree.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
bash "$root/.github/scripts/ensure-bootstrap.sh"

host="$(mktemp)"
cp -a "$root/Build/bootstrap/robix-run.app" "$host"
chmod 755 "$host"
project="$root/Robix.rxp"
backup="$(mktemp)"
cp "$project" "$backup"
restore_project() {
    cp "$backup" "$project"
    rm -f "$backup" "$host"
}
trap restore_project EXIT

build_named() {
    local name="$1"
    local output="$2"
    rm -f "$root/Build/temp"/*.o
    sed "s/Name: \"Robix\";/Name: \"${name}\";/" "$backup" > "$project"
    (
        cd "$root"
        "$host" Build --Configuration Development --NoCache
    )
    if [[ ! -x "$output" ]]; then
        echo "error: the host build did not write ${output}" >&2
        exit 1
    fi
}

build_named "Robix" "$root/Build/Compiler/Robix.app"
cp -a "$root/Build/Compiler/Robix.app" "$root/Build/bootstrap/robix-run.app"
chmod 755 "$root/Build/bootstrap/robix-run.app"

build_named "Robixc" "$root/Build/Compiler/Robixc.app"
cp -a "$root/Build/Compiler/Robixc.app" "$root/Build/bootstrap/robixc.app"
chmod 755 "$root/Build/bootstrap/robixc.app"
echo "recompiled the Linux interpreter and compiler" >&2
