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

$UiInclude = Join-Path $Root "RobixUI"
$CompileKey = "-std=c++20 -O2 -ffunction-sections -fdata-sections -I RobixUI"
$script:Compiled = 0
$script:Skipped = 0

function Get-MakeDeps([string]$DepFile) {
    if (-not (Test-Path -LiteralPath $DepFile)) { return @() }
    $text = [IO.File]::ReadAllText($DepFile) -replace "\\\r?\n", " "
    $mark = [regex]::Match($text, "\.(?:obj|o):\s*([\s\S]*)$")
    if (-not $mark.Success) { return @() }
    $rest = $mark.Groups[1].Value
    $paths = New-Object System.Collections.Generic.List[string]
    $current = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $rest.Length; $i++) {
        $ch = $rest[$i]
        if ($ch -eq '\' -and ($i + 1) -lt $rest.Length) {
            $next = $rest[$i + 1]
            if ($next -eq ' ' -or $next -eq '\') {
                [void]$current.Append($next)
                $i++
                continue
            }
        }
        if ([char]::IsWhiteSpace($ch)) {
            if ($current.Length -gt 0) {
                $paths.Add($current.ToString())
                [void]$current.Clear()
            }
            continue
        }
        [void]$current.Append($ch)
    }
    if ($current.Length -gt 0) { $paths.Add($current.ToString()) }
    return $paths
}

function Test-AnyNewer([string]$Output, [string[]]$Inputs) {
    $outputTime = (Get-Item -LiteralPath $Output).LastWriteTimeUtc
    foreach ($path in $Inputs) {
        if ([string]::IsNullOrWhiteSpace($path)) { continue }
        if (-not (Test-Path -LiteralPath $path)) { continue }
        if ((Get-Item -LiteralPath $path).LastWriteTimeUtc -gt $outputTime) { return $true }
    }
    return $false
}

function Update-DepFile([string]$Source, [string]$DepFile) {
    & $Clang -std=c++20 -I $UiInclude -MM -MF $DepFile $Source
    return $LASTEXITCODE -eq 0
}

function Compile-Unit([string]$Source, [string]$Object) {
    $rel = $Source.Substring($Root.Length + 1)
    $depFile = [IO.Path]::ChangeExtension($Object, ".d")
    $flagFile = [IO.Path]::ChangeExtension($Object, ".flags")
    $flagsDiffer = $false
    if (Test-Path -LiteralPath $flagFile) {
        $flagsDiffer = ([IO.File]::ReadAllText($flagFile) -ne $CompileKey)
    }
    $needs = -not (Test-Path -LiteralPath $Object) -or $flagsDiffer
    if (-not $needs) {
        $needs = (Get-Item -LiteralPath $Source).LastWriteTimeUtc -gt (Get-Item -LiteralPath $Object).LastWriteTimeUtc
    }
    if (-not $needs) {
        if (-not (Test-Path -LiteralPath $depFile)) {
            [void](Update-DepFile $Source $depFile)
        }
        $deps = @(Get-MakeDeps $depFile)
        if ($deps.Count -gt 0) {
            $needs = Test-AnyNewer $Object $deps
        }
    }
    if (-not $needs) {
        if (-not (Test-Path -LiteralPath $flagFile)) {
            [IO.File]::WriteAllText($flagFile, $CompileKey)
        }
        Write-Host "  skip $rel"
        $script:Skipped++
        return
    }
    Write-Host "  compile $rel"
    & $Clang -std=c++20 -O2 -ffunction-sections -fdata-sections -I $UiInclude -MD -MF $depFile -c $Source -o $Object
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $rel" }
    [IO.File]::WriteAllText($flagFile, $CompileKey)
    $script:Compiled++
}

$sources = @(Get-CompilerSources)
$objects = @()
$usedNames = @{}
$mainSources = @(
    (Join-Path $Root "Tools\CLI\RobixcMain.cpp"),
    (Join-Path $Root "Tools\CLI\RobixMain.cpp")
)
Write-Host "Compiling $($sources.Count + $mainSources.Count) compiler units, one object each"
foreach ($file in @($sources + @(Get-Item -LiteralPath $mainSources))) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($file.Name)
    if ($usedNames.ContainsKey($stem)) {
        throw "Two compiler sources share the object name $stem.obj"
    }
    $usedNames[$stem] = $file.FullName
    $object = Join-Path $Obj ($stem + ".obj")
    if ($mainSources -notcontains $file.FullName) {
        $objects += $object
    }
    Compile-Unit $file.FullName $object
}
Write-Host "  compiled $script:Compiled, skipped $script:Skipped unchanged"

$mains = @(
    @{ Source = "Tools\CLI\RobixcMain.cpp"; Output = "Robixc.exe" },
    @{ Source = "Tools\CLI\RobixMain.cpp"; Output = "Robix.exe" }
)
$linkLibs = @(
    "-lws2_32", "-lsecur32", "-lcrypt32", "-luser32", "-lgdi32", "-lshell32",
    "-ldwrite", "-ld3d11", "-ldxgi", "-lole32", "-loleaut32", "-lwindowscodecs", "-lwinhttp", "-liphlpapi"
)
$linkFailed = @()
foreach ($main in $mains) {
    $source = Join-Path $Root $main.Source
    $mainObj = Join-Path $Obj ([IO.Path]::GetFileNameWithoutExtension($source) + ".obj")
    $exe = Join-Path $Bin $main.Output
    $linkInputs = @($objects + $mainObj)
    $linkCurrent = (Test-Path -LiteralPath $exe) -and -not (Test-AnyNewer $exe $linkInputs)
    if ($linkCurrent) {
        Write-Host "  skip link $($main.Output)"
        continue
    }
    Write-Host "Linking $($main.Output)"
    & $Clang -O2 @linkInputs -o $exe @linkLibs
    if ($LASTEXITCODE -ne 0) {
        $linkFailed += $main.Output
        Write-Host "  $($main.Output) was not replaced. Close it if it is running, then run this build again."
    }
}
$runtimeNames = @("RobixNativeRuntime.obj", "LoopbackText.obj", "EnvStore.obj", "NetworkRuntime.obj")
foreach ($name in $runtimeNames) {
    $from = Join-Path $Obj $name
    if (-not (Test-Path -LiteralPath $from)) { throw "missing runtime object $name" }
    $dest = Join-Path $Lib $name
    $copy = -not (Test-Path -LiteralPath $dest)
    if (-not $copy) {
        $copy = (Get-Item -LiteralPath $from).LastWriteTimeUtc -gt (Get-Item -LiteralPath $dest).LastWriteTimeUtc
    }
    if ($copy) {
        Copy-Item -LiteralPath $from -Destination $dest -Force
        Write-Host "  install $name"
    } else {
        Write-Host "  skip $name"
    }
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
        if ((Test-Path -LiteralPath $dest) -and (Get-Item -LiteralPath $dest).LastWriteTimeUtc -ge $_.LastWriteTimeUtc) {
            return
        }
        Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
    }
}

Copy-Tree (Join-Path $Root "Runtime") (Join-Path $Include "Runtime") "*.hpp"
Copy-Tree (Join-Path $Root "StdLib") (Join-Path $Include "StdLib") "*.hpp"
Copy-Tree (Join-Path $Root "StdLib\Robix") (Join-Path $Packages "Robix.Core") "*.rx"
Copy-Tree (Join-Path $Root "RobixUI\Compiler\Components") (Join-Path $Share "Schemas") "*.rxu"
$foundation = Join-Path $Root "RobixUI\Compiler\Foundation\RoboFoundation.xml"
if (Test-Path -LiteralPath $foundation) {
    $foundationDir = Join-Path $Share "Foundation"
    New-Item -ItemType Directory -Force -Path $foundationDir | Out-Null
    Copy-Item -LiteralPath $foundation -Destination (Join-Path $foundationDir "RoboFoundation.xml") -Force
}
function Copy-ToolchainFile([string]$From, [string]$To) {
    if (-not (Test-Path -LiteralPath $From)) { throw "missing toolchain file $From" }
    $copy = -not (Test-Path -LiteralPath $To)
    if (-not $copy) {
        $sourceInfo = Get-Item -LiteralPath $From
        $destInfo = Get-Item -LiteralPath $To
        $copy = $sourceInfo.Length -ne $destInfo.Length -or $sourceInfo.LastWriteTimeUtc -gt $destInfo.LastWriteTimeUtc
    }
    if (-not $copy) { return }
    $parent = Split-Path -Parent $To
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    Copy-Item -LiteralPath $From -Destination $To -Force
    Write-Host "  toolchain $([IO.Path]::GetFileName($To))"
}

$toolchain = Join-Path $Bin "toolchain"
$clangBin = Split-Path -Parent $Clang
foreach ($toolName in @("clang.exe", "opt.exe", "ld.lld.exe", "llvm-ar.exe", "llvm-nm.exe", "llvm-objcopy.exe")) {
    Copy-ToolchainFile (Join-Path $clangBin $toolName) (Join-Path $toolchain $toolName)
}
$msvcRoots = @(
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
)
$msvcLib = $null
foreach ($root in $msvcRoots) {
    if (-not (Test-Path -LiteralPath $root)) { continue }
    $latest = Get-ChildItem -LiteralPath $root -Directory | Sort-Object Name -Descending | Select-Object -First 1
    if ($latest) { $msvcLib = Join-Path $latest.FullName "lib\x64"; break }
}
if (-not $msvcLib) { throw "The compiler build needs the MSVC x64 libraries once, so they can be copied into the compiler." }
$kitRoot = "${env:ProgramFiles(x86)}\Windows Kits\10\Lib"
$kitVersion = Get-ChildItem -LiteralPath $kitRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
if (-not $kitVersion) { throw "The compiler build needs the Windows SDK libraries once, so they can be copied into the compiler." }
$toolLib = Join-Path $toolchain "lib"
foreach ($name in @("libcmt.lib", "libvcruntime.lib", "libcpmt.lib", "oldnames.lib")) {
    Copy-ToolchainFile (Join-Path $msvcLib $name) (Join-Path $toolLib $name)
}
Copy-ToolchainFile (Join-Path $kitVersion.FullName "ucrt\x64\libucrt.lib") (Join-Path $toolLib "libucrt.lib")
foreach ($name in @("kernel32.lib", "ws2_32.lib", "secur32.lib", "crypt32.lib")) {
    Copy-ToolchainFile (Join-Path $kitVersion.FullName "um\x64\$name") (Join-Path $toolLib $name)
}
$builtins = Join-Path $clangBin "..\lib\clang\18\lib\windows\clang_rt.builtins-x86_64.lib"
if (Test-Path -LiteralPath $builtins) {
    Copy-ToolchainFile $builtins (Join-Path $toolLib "clang_rt.builtins-x86_64.lib")
}
foreach ($name in $runtimeNames) {
    Copy-ToolchainFile (Join-Path $Obj $name) (Join-Path $Bin $name)
}

Set-Content -LiteralPath (Join-Path $Share "Version.txt") -Value "Robix $Version`nRoboticela`n" -Encoding ascii
Set-Content -LiteralPath (Join-Path $Bin "Toolchain.txt") -Value (Join-Path $toolchain "clang.exe") -Encoding ascii

Write-Host ""
Write-Host "Robix $Version"
Write-Host "  Bin      $Bin"
Write-Host "  Lib      $Lib"
Write-Host "  Include  $Include"
Write-Host "  Packages $Packages"
Write-Host "  Share    $Share"
Get-ChildItem -LiteralPath $Bin -File | ForEach-Object { Write-Host "  program  $($_.Name)" }
if ($linkFailed.Count -gt 0) {
    throw "link failed: $($linkFailed -join ', '). Close the running program and run this build again."
}
