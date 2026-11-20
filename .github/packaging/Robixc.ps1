$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Engine = Join-Path $Root "Shared\Tools\Compiler.exe"
$Lld = Join-Path $Root "Shared\Tools\Llvm\bin\ld.lld.exe"
$Runtime = Join-Path $Root "Library\Runtime.lib"
$Lib = Join-Path $Root "Shared\Tools\Llvm\x86_64-w64-mingw32\lib"

function Show-Usage {
    Write-Error "Robixc compiles every Robix file in a folder into one program.`nUsage: Robixc <Folder> <Output>`n       Robixc <File.rx> [More.rx ...] <Output>"
    exit 1
}

if ($args.Count -lt 2 -or -not (Test-Path $Engine) -or -not (Test-Path $Lld)) {
    Show-Usage
}

$sources = @()
if (Test-Path $args[0] -PathType Container) {
    if ($args.Count -ne 2) {
        Write-Error "error: a folder compile takes the folder and the output program"
        exit 1
    }
    $sources = Get-ChildItem -Path $args[0] -File -Filter *.rx | Where-Object { $_.Name -notlike "*.test.rx" } | Sort-Object Name
    $output = $args[1]
} else {
    $output = $args[$args.Count - 1]
    for ($index = 0; $index -lt $args.Count - 1; $index++) {
        if (-not (Test-Path $args[$index] -PathType Leaf)) {
            Write-Error "error: source was not found: $($args[$index])"
            exit 1
        }
        $sources += Get-Item $args[$index]
    }
}

if ($sources.Count -eq 0) {
    Write-Error "error: the folder has no Robix files"
    exit 1
}

$entry = $sources | Where-Object { Select-String -Path $_.FullName -Pattern "Function Main" -Quiet } | Select-Object -First 1
if (-not $entry) { $entry = $sources[0] }

$work = Join-Path ([System.IO.Path]::GetTempPath()) ("robixc-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $work | Out-Null
$objects = @()
try {
    $index = 0
    foreach ($source in $sources) {
        $index++
        $object = Join-Path $work ("{0}-{1}.o" -f $index, $source.BaseName)
        $env:ROBIX_OBJECT_ONLY = "1"
        $env:ROBIX_KEEP_MAIN = $(if ($source.FullName -eq $entry.FullName) { "1" } else { "0" })
        $env:PATH = "$(Join-Path $Root 'Shared\Tools');$env:PATH"
        & $Engine $source.FullName $object
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        $objects += $object
    }
    $builtins = Get-ChildItem -Path (Join-Path $Root "Shared\Tools\Llvm\lib\clang") -Recurse -Filter "libclang_rt.builtins-x86_64.a" | Select-Object -First 1
    & $Lld -m i386pep -o $output `
        (Join-Path $Lib "crt2.o") `
        (Join-Path $Lib "crtbegin.o") `
        @objects `
        $Runtime `
        "-L$Lib" `
        "-L$(Join-Path $Lib '..\mingw\lib')" `
        "-lc++" "-lmingw32" $builtins.FullName `
        "-lunwind" "-lmoldname" "-lmingwex" "-lmsvcrt" "-ladvapi32" "-lshell32" "-luser32" "-lkernel32" `
        "-lmingw32" $builtins.FullName `
        "-lunwind" "-lmoldname" "-lmingwex" "-lmsvcrt" "-lkernel32" `
        (Join-Path $Lib "crtend.o")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Remove-Item -Recurse -Force $work
}
