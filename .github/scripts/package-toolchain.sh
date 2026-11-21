#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
stage="$root/dist/Robix"
rm -rf "$stage"
mkdir -p "$stage/Bin" "$stage/Packages/Builtin" "$stage/Packages/Installed" \
    "$stage/Library" "$stage/Runtime" "$stage/Shared/Tools" \
    "$stage/Shared/Syntax" "$stage/Shared/Runtime" "$stage/Shared/Build" "$stage/Shared/UI"

shopt -s nullglob
for tool in llc clang++ ld.lld; do
    if command -v "$tool" >/dev/null 2>&1; then
        continue
    fi
    for candidate in /usr/bin/"$tool"-* /usr/lib/llvm-*/bin/"$tool"; do
        if [[ -x "$candidate" ]]; then
            ln -sfn "$candidate" "/tmp/$tool"
            export PATH="/tmp:$PATH"
            break
        fi
    done
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: $tool was not found on the build machine" >&2
        exit 1
    fi
done

bash "$root/.github/scripts/bundle-llvm.sh" "$stage/Shared/Tools/Llvm" "$stage/Library/Link"
install -m 755 "$root/.github/packaging/clang++" "$stage/Shared/Tools/clang++"
clang++ -std=c++17 -O3 -fPIC -c -o "$stage/Runtime/Runtime.o" "$root/Runtime/Runtime.cpp"
ar rcs "$stage/Library/Runtime.lib" "$stage/Runtime/Runtime.o"

install -m 755 "$root/Build/bootstrap/robix-run.app" "$stage/Bin/Robix"
install -m 755 "$root/Build/bootstrap/robixc.app" "$stage/Shared/Tools/Compiler"
install -m 755 "$root/.github/packaging/Robixc" "$stage/Bin/Robixc"

place_bin() {
    local source="$1"
    local area="$2"
    local package="$3"
    local name
    name="$(basename "$source")"
    if [[ ! -f "$source" ]]; then
        return 0
    fi
    install -m 644 "$source" "$stage/Shared/$area/$name"
    mkdir -p "$stage/Packages/Builtin/$package"
    install -m 644 "$source" "$stage/Packages/Builtin/$package/$name"
}

syntax=(
    Lexer.bin Parser.bin ParserExpressions.bin ParserStatements.bin
    Token.bin TokenKind.bin TypeChecker.bin NameChecker.bin
    NativeEmit.bin Emitter.bin SyntaxCompiler.bin Diagnostic.bin
    AstNode.bin TextConvert.bin SyntaxCoverage.bin
)
for name in "${syntax[@]}"; do
    place_bin "$root/Build/Compiler/$name" Syntax Robix.Syntax
done
place_bin "$root/Build/Compiler/Interpreter.bin" Runtime Robix.Runtime
place_bin "$root/Build/Compiler/ProjectSystem.bin" Build Robix.Build
place_bin "$root/Build/Compiler/Robixc.bin" Tools Robix.Tools

mkdir -p "$stage/Packages/Builtin/Robix.UI" "$stage/Shared/UI"

if find "$stage" \( -path '*/Llvm/*' -o -path '*/Link/*' \) -prune -o \( -name '*.cpp' -o -name '*.cc' -o -name '*.rx' \) -print | grep -q .; then
    echo "error: the package still contains source files" >&2
    find "$stage" -name '*.cpp' -o -name '*.cc' -o -name '*.rx'
    exit 1
fi

bash "$root/.github/scripts/install-docs.sh" "$stage"

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
"$stage/Bin/Robixc" "$folder" "$folder/Hello"
"$folder/Hello" | grep -q "Hello, Robix"
rm -rf "$folder"

bash "$root/.github/scripts/package-update.sh" "$stage" linux-x64

archive="$root/dist/robix-linux-x64.zip"
rm -f "$archive"
(
    cd "$root/dist"
    zip -r "robix-linux-x64.zip" Robix
)
file_list=$(unzip -l "$archive" | tr '\\' '/')
for required in 'Robix/README.md' 'Robix/LICENSE' 'Robix/Shared/Tools/Llvm/LLVM-LICENSE' 'Robix/Library/Link/LICENSE-GCC' 'Robix/Library/Link/LICENSE-GLIBC'; do
    if [[ "$file_list" != *"$required"* ]]; then
        echo "error: ${archive} is missing ${required}" >&2
        exit 1
    fi
done
echo "Packaged $archive"
if [[ "${SKIP_WINDOWS:-}" != "1" ]]; then
    bash "$root/.github/scripts/package-windows.sh"
fi
