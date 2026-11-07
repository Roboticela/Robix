$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $root
$robix = Join-Path $root "Build\bin\robix.exe"
if (-not (Test-Path $robix)) {
    Write-Error "robix.exe was not found. Build it first."
    exit 1
}
& $robix ui web "Examples\UiDemo\MainWindow.rbxu" --code "Examples\UiDemo\Main.rbx" --dev
exit $LASTEXITCODE
