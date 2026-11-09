$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Clang = "C:\CLang\clang+llvm-18.1.8-x86_64-pc-windows-msvc\bin\clang++.exe"
if (-not (Test-Path -LiteralPath $Clang)) {
    $Clang = "clang++"
}

$Version = "1.0.0"
$Prefix = Join-Path $Root "Build"
$Bin = Join-Path $Prefix "Bin"
$Obj = Join-Path $Prefix "Obj"
$Lib = Join-Path $Prefix "Lib\Robix\$Version"
$Include = Join-Path $Prefix "Include\Robix"
$Packages = Join-Path $Prefix "Packages"
$Share = Join-Path $Prefix "Share\Robix"

foreach ($dir in @($Bin, $Obj, $Lib, $Include, $Packages, $Share)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

$SkipNames = @{
    "RobixcMain.cpp" = $true
    "RobixMain.cpp" = $true
    "ContainerPlatform.cpp" = $true
    "RoboGPUDevice.cpp" = $true
}

function Get-CompilerSources {
    $roots = @(
        "Compiler", "Interpreter", "Network", "Packages", "Runtime", "StdLib", "RobixUI", "Robix3D", "Tools"
    )
    $files = @()
    foreach ($rel in $roots) {
        $dir = Join-Path $Root $rel
        if (-not (Test-Path -LiteralPath $dir)) { continue }
        $files += @(Get-ChildItem -LiteralPath $dir -Recurse -Filter *.cpp -File)
    }
    $kept = @()
    foreach ($file in $files) {
        if ($SkipNames.ContainsKey($file.Name)) { continue }
        $full = $file.FullName
        if ($full -match "\\Platforms\\(Android|IOS|Linux|MacOS)\\") { continue }
        if ($full -match "\\Components\\.*Platform\.cpp$") { continue }
        if ($full -match "\\RoboWindowsMain\.cpp$") { continue }
        $kept += $file
    }
    return $kept
}

$sources = @(Get-CompilerSources)
$objects = @()
$usedNames = @{}
Write-Host "Compiling $($sources.Count) compiler units"
foreach ($file in $sources) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($file.Name)
    if ($usedNames.ContainsKey($stem)) {
        throw "Two compiler sources share the object name $stem.obj"
    }
    $usedNames[$stem] = $file.FullName
    $object = Join-Path $Obj ($stem + ".obj")
    $objects += $object
    $newer = -not (Test-Path -LiteralPath $object)
    if (-not $newer) {
        $newer = (Get-Item -LiteralPath $file.FullName).LastWriteTimeUtc -gt (Get-Item -LiteralPath $object).LastWriteTimeUtc
    }
    if (-not $newer) { continue }
    Write-Host "  $($file.FullName.Substring($Root.Length + 1))"
    & $Clang -std=c++20 -O2 -ffunction-sections -fdata-sections -I (Join-Path $Root "RobixUI") -c $file.FullName -o $object
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $($file.Name)" }
}

$mains = @(
    @{ Source = "Tools\CLI\RobixcMain.cpp"; Output = "Robixc.exe" },
    @{ Source = "Tools\CLI\RobixMain.cpp"; Output = "Robix.exe" }
)
$linkLibs = @(
    "-lws2_32", "-lsecur32", "-lcrypt32", "-luser32", "-lgdi32", "-lshell32",
    "-ldwrite", "-ld3d11", "-ldxgi", "-lole32", "-loleaut32", "-lwindowscodecs", "-lwinhttp", "-liphlpapi"
)
foreach ($main in $mains) {
    $source = Join-Path $Root $main.Source
    $mainObj = Join-Path $Obj ([IO.Path]::GetFileNameWithoutExtension($source) + ".obj")
    Write-Host "  $($main.Source)"
    & $Clang -std=c++20 -O2 -ffunction-sections -fdata-sections -c $source -o $mainObj
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $($main.Source)" }
    $exe = Join-Path $Bin $main.Output
    Write-Host "Linking $($main.Output)"
    & $Clang -O2 @objects $mainObj -o $exe @linkLibs
    if ($LASTEXITCODE -ne 0) { throw "link failed: $($main.Output)" }
}

$runtimeNames = @("RobixNativeRuntime.obj", "LoopbackText.obj", "EnvStore.obj", "NetworkRuntime.obj")
foreach ($name in $runtimeNames) {
    $from = Join-Path $Obj $name
    if (-not (Test-Path -LiteralPath $from)) { throw "missing runtime object $name" }
    Copy-Item -LiteralPath $from -Destination (Join-Path $Lib $name) -Force
}
foreach ($import in @("Robixc.lib", "Robix.lib", "Robixc.exp", "Robix.exp")) {
    $from = Join-Path $Bin $import
    if (Test-Path -LiteralPath $from) {
        Move-Item -LiteralPath $from -Destination (Join-Path $Lib $import) -Force
    }
}

function Copy-Tree([string]$From, [string]$To, [string]$Filter) {
    if (-not (Test-Path -LiteralPath $From)) { return }
    New-Item -ItemType Directory -Force -Path $To | Out-Null
    Get-ChildItem -LiteralPath $From -Recurse -File -Filter $Filter | ForEach-Object {
        $rel = $_.FullName.Substring($From.Length).TrimStart("\")
        $dest = Join-Path $To $rel
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null
        Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
    }
}

Copy-Tree (Join-Path $Root "Runtime") (Join-Path $Include "Runtime") "*.hpp"
Copy-Tree (Join-Path $Root "StdLib") (Join-Path $Include "StdLib") "*.hpp"
Copy-Tree (Join-Path $Root "StdLib\Robix") (Join-Path $Packages "Robix.Core") "*.rbx"
Copy-Tree (Join-Path $Root "RobixUI\Compiler\Components") (Join-Path $Share "Schemas") "*.rbxu"
$foundation = Join-Path $Root "RobixUI\Compiler\Foundation\RoboFoundation.xml"
if (Test-Path -LiteralPath $foundation) {
    $foundationDir = Join-Path $Share "Foundation"
    New-Item -ItemType Directory -Force -Path $foundationDir | Out-Null
    Copy-Item -LiteralPath $foundation -Destination (Join-Path $foundationDir "RoboFoundation.xml") -Force
}
Set-Content -LiteralPath (Join-Path $Share "Version.txt") -Value "Robix $Version`nRoboticela`n" -Encoding ascii
Set-Content -LiteralPath (Join-Path $Bin "Toolchain.txt") -Value $Clang -Encoding ascii

Write-Host ""
Write-Host "Robix $Version"
Write-Host "  Bin      $Bin"
Write-Host "  Lib      $Lib"
Write-Host "  Include  $Include"
Write-Host "  Packages $Packages"
Write-Host "  Share    $Share"
Get-ChildItem -LiteralPath $Bin -File | ForEach-Object { Write-Host "  program  $($_.Name)" }
