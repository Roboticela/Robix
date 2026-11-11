param(
    [int]$Port = 4173
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Robix = Join-Path $Root "Build\Compiler\Robix.exe"
$Document = Join-Path $Root "DevKit\Main.rxu"
if (-not (Test-Path -LiteralPath $Robix)) {
    throw "Build\Compiler\Robix.exe is missing. Run Scripts\BuildCompiler.ps1 first."
}
& $Robix run $Document --target web --watch --port $Port
if ($LASTEXITCODE -ne 0) { throw "DevKit interpreter failed" }
