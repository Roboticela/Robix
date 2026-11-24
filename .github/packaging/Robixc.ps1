$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Engine = Join-Path $Root "Shared\Tools\Compiler.exe"

function Show-Usage {
    Write-Error "Robixc compiles a Robix file into a machine-code program.`nUsage: Robixc <File.rx> <Output>`n       Robixc <Folder> <Output>"
    exit 1
}

if ($args.Count -lt 2 -or -not (Test-Path $Engine)) {
    Show-Usage
}

$Source = $args[0]
$Output = $args[1]
if (Test-Path $Source -PathType Container) {
    $Main = Get-ChildItem -Path $Source -Filter *.rx -File | Where-Object { $_.Name -notlike "*.test.rx" -and (Select-String -Path $_.FullName -Pattern "Function Main" -Quiet) } | Select-Object -First 1
    if ($null -eq $Main) {
        Write-Error "error: the folder has no Robix program"
        exit 1
    }
    $Source = $Main.FullName
}

if (-not (Test-Path $Source -PathType Leaf)) {
    Write-Error "error: source was not found: $Source"
    exit 1
}

& $Engine $Source $Output "--target" "windows-x64"
exit $LASTEXITCODE
