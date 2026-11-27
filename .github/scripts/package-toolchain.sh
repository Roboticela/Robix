#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
stage="$root/dist/Robix"
bash "$root/.github/scripts/build-host.sh"
bash "$root/.github/scripts/stage-layout.sh" "$stage" linux-x64 \
    "$root/Build/Compiler/Robix.app" \
    "$root/Build/Compiler/Robixc.app" \
    "$root/Build/bootstrap/robix-run.app"
rm -rf "$root/Build/bootstrap"

hello="$root/Examples/HelloRobot.rx"
"$stage/Bin/Robix" run "$hello" | grep -q "Hello, Robix"

folder="$(mktemp -d)"
cp "$root/Examples/HelloRobot.rx" "$root/Examples/HelloRobot.test.rx" "$folder/"
cat > "$folder/Extra.rx" <<'EOF'
Package Robotics.Extra;

Public Function Extra() -> Int
Explain {
    Description: "Return one.";
    Processing: "Return the value.";
    Output: "The value one.";
} {
    Return 1;
}
EOF
cat > "$folder/Extra.test.rx" <<'EOF'
Test Extra {
    Constraint: True;
    Processing: True;
    Output: True;
}
EOF
(
    cd "$folder"
    "$stage/Bin/Robixc" HelloRobot.rx Hello --target linux-x64
)
chmod +x "$folder/Hello"
"$folder/Hello" | grep -q "Hello, Robix"
rm -rf "$folder"

archive="$root/dist/robix-linux-x64.zip"
rm -f "$archive"
(
    cd "$root/dist"
    zip -r "robix-linux-x64.zip" Robix
)
file_list=$(unzip -l "$archive" | tr '\\' '/')
for required in 'Robix/README.md' 'Robix/LICENSE' 'Robix/Bin/Robix' 'Robix/Bin/Robixc' 'Robix/Library/Lexer.o' 'Robix/Packages/Builtin/Robix.Core/Robix.Core.rx' 'Robix/Packages/Builtin/Robix.Core/Robix.Core.o' 'Robix/Documentation/'; do
    if [[ "$file_list" != *"$required"* ]]; then
        echo "error: ${archive} is missing ${required}" >&2
        exit 1
    fi
done
echo "Packaged $archive"
if [[ "${SKIP_WINDOWS:-}" != "1" ]]; then
    bash "$root/.github/scripts/package-windows.sh"
fi
