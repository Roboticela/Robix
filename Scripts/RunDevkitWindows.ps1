$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Robix = Join-Path $Root "Build\Compiler\Robix.exe"
$Document = Join-Path $Root "DevKit\Main.rxu"
if (-not (Test-Path -LiteralPath $Robix)) {
    throw "Build\Compiler\Robix.exe is missing. Run Scripts\BuildCompiler.ps1 first."
}
& $Robix run $Document --target windows --watch
if ($LASTEXITCODE -ne 0) { throw "DevKit interpreter failed" }
