#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"

if [[ -n "${ROBIX_VERSION:-}" ]]; then
    version="$ROBIX_VERSION"
elif [[ "${GITHUB_REF_TYPE:-}" == "tag" && "${GITHUB_REF_NAME:-}" == v* ]]; then
    version="${GITHUB_REF_NAME#v}"
else
    version="$(tr -d '[:space:]' < "$root/VERSION")"
fi

if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z.]+)?$ ]]; then
    echo "error: invalid Robix version: ${version}" >&2
    exit 1
fi

printf '%s\n' "$version"
