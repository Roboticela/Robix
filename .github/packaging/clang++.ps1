$ErrorActionPreference = "Stop"
$Here = Split-Path -Parent $PSScriptRoot
$Root = $Here
while (-not (Test-Path (Join-Path $Root "Shared\Tools\Llvm\bin\llc.exe"))) {
    $parent = Split-Path -Parent $Root
    if ($parent -eq $Root) {
        Write-Error "error: the bundled llc is missing"
        exit 1
    }
    $Root = $parent
}
if ($env:ROBIX_OBJECT_ONLY -ne "1") {
    Write-Error "error: machine code is generated with llc, not clang++"
    exit 1
}
$out = ""
$ll = ""
$previous = ""
foreach ($argument in $args) {
    if ($previous -eq "-o") { $out = $argument }
    if ($argument -like "*.ll") { $ll = $argument }
    $previous = $argument
}
if ($out -eq "" -or $ll -eq "") {
    Write-Error "error: the compiler did not provide an object and an IR file"
    exit 1
}
if ($env:ROBIX_KEEP_MAIN -ne "1") {
    $skip = $false
    $depth = 0
    $kept = @()
    foreach ($line in Get-Content -Path $ll) {
        if (-not $skip -and $line.StartsWith("define i32 @main")) {
            $skip = $true
            $depth = 0
        }
        if ($skip) {
            $opens = ([regex]::Matches($line, "\{")).Count
            $closes = ([regex]::Matches($line, "\}")).Count
            $depth += $opens - $closes
            if (($opens + $closes) -gt 0 -and $depth -le 0) { $skip = $false }
            continue
        }
        $kept += $line
    }
    Set-Content -Path $ll -Value $kept
}
$llc = Join-Path $Root "Shared\Tools\Llvm\bin\llc.exe"
& $llc -filetype=obj -O3 -relocation-model=pic -mtriple=x86_64-w64-windows-gnu -o $out $ll
exit $LASTEXITCODE
