#!/usr/bin/env bash
# Prefer Robix/Bin/RobixBootstrap from the latest GitHub release.
# If that asset is missing or cannot run, compile Bootstrap/main.cpp.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
dest="$root/Build/bootstrap/robix-run.app"
mkdir -p "$root/Build/bootstrap"

host_asset() {
    local os="linux"
    local arch="x64"
    local machine
    machine="$(uname -m)"
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) os="windows" ;;
    esac
    case "$machine" in
        x86_64|amd64) arch="x64" ;;
        aarch64|arm64) arch="arm64" ;;
        armv7l|armv6l|arm) arch="arm" ;;
        i686|i386) arch="x86" ;;
        riscv64) arch="riscv64" ;;
        ppc64le) arch="ppc64le" ;;
        s390x) arch="s390x" ;;
        loongarch64) arch="loongarch64" ;;
    esac
    printf 'robix-%s-%s.zip' "$os" "$arch"
}

try_release() {
    local asset zip
    asset="$(host_asset)"
    zip="$root/Build/bootstrap/release.zip"
    if ! command -v python3 >/dev/null 2>&1; then
        return 1
    fi
    if ! python3 - "$asset" "$zip" <<'PY'
import json, os, sys, urllib.request
asset, dest = sys.argv[1], sys.argv[2]
repo = os.environ.get("ROBIX_GITHUB_REPO", "Roboticela/Robix")
token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN") or ""
headers = {"Accept": "application/vnd.github+json", "User-Agent": "robix-bootstrap"}
if token:
    headers["Authorization"] = "Bearer " + token
url = "https://api.github.com/repos/" + repo + "/releases/latest"
try:
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=20) as response:
        release = json.load(response)
except Exception:
    sys.exit(1)
chosen = next((item for item in release.get("assets", []) if item.get("name") == asset), None)
if chosen is None:
    sys.exit(1)
download = dict(headers)
download["Accept"] = "application/octet-stream"
try:
    with urllib.request.urlopen(urllib.request.Request(chosen["url"], headers=download), timeout=120) as response:
        data = response.read()
except Exception:
    sys.exit(1)
if len(data) < 1000:
    sys.exit(1)
with open(dest, "wb") as output:
    output.write(data)
PY
    then
        return 1
    fi
    local member="Robix/Bin/RobixBootstrap"
    if [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* || "$(uname -s)" == CYGWIN* ]]; then
        member="Robix/Bin/RobixBootstrap.exe"
    fi
    rm -f "$dest"
    if ! unzip -p "$zip" "$member" > "$dest" 2>/dev/null; then
        rm -f "$dest" "$zip"
        return 1
    fi
    rm -f "$zip"
    chmod 755 "$dest"
    if [[ ! -s "$dest" ]]; then
        rm -f "$dest"
        return 1
    fi
    if ! "$dest" --targets >/dev/null 2>&1; then
        rm -f "$dest"
        return 1
    fi
    echo "bootstrap: using ${asset} from the latest GitHub release" >&2
    return 0
}

compile_cpp() {
    # ROBIX_CXX is the compiler for the target being packaged. This binary
    # has to run on the machine that is doing the build.
    local cxx="${ROBIX_HOST_CXX:-}"
    if [[ -z "$cxx" ]]; then
        if command -v clang++ >/dev/null 2>&1; then
            cxx="clang++"
        elif command -v g++ >/dev/null 2>&1; then
            cxx="g++"
        else
            echo "error: clang++ or g++ is required to build the bootstrap" >&2
            exit 1
        fi
    fi
    echo "bootstrap: compiling Bootstrap/main.cpp with ${cxx}" >&2
    "$cxx" -std=c++20 -O2 -o "$dest" "$root/Bootstrap/main.cpp"
    chmod 755 "$dest"
    if ! "$dest" --targets >/dev/null 2>&1; then
        echo "error: the bootstrap compiler cannot run on this machine" >&2
        exit 1
    fi
}

if ! try_release; then
    compile_cpp
fi
if [[ ! -x "$dest" ]]; then
    echo "error: $dest was not created" >&2
    exit 1
fi
