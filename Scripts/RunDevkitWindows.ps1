param(
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Document = Join-Path $Root "DevKit\Main.xml"
$Robix = Join-Path $Root "Build\bin\robix-ui.exe"
if (-not (Test-Path -LiteralPath $Robix)) {
    $Robix = Join-Path $Root "Build\bin\robix.exe"
}
if (-not (Test-Path -LiteralPath $Robix)) {
    throw "robix-ui.exe is missing. Build the Robix tools before running DevKit."
}
if (-not (Test-Path -LiteralPath $Document)) {
    throw "DevKit document missing: $Document"
}

Write-Host "DevKit window"
& $Robix ui window $Document --dev
if ($LASTEXITCODE -ne 0) { throw "DevKit window failed" }
