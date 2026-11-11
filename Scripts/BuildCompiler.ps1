$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Clang = "C:\CLang\clang+llvm-18.1.8-x86_64-pc-windows-msvc\bin\clang++.exe"
if (-not (Test-Path -LiteralPath $Clang)) {
    $Clang = "clang++"
}

$Version = "1.0.0"
$Prefix = Join-Path $Root "Build"
$Compiler = Join-Path $Prefix "Compiler"
$Units = Join-Path $Compiler "Units"
$Programs = $Compiler
$Toolchain = Join-Path $Compiler "Toolchain"
$ToolLibraries = Join-Path $Toolchain "Libraries"
$Runtime = Join-Path $Compiler "Runtime"
$ImportLibraries = Join-Path $Compiler "Libraries"
$LegacyObj = Join-Path $Prefix "Obj"
$Include = Join-Path $Compiler "Include"
$Packages = Join-Path $Compiler "Packages"
$Share = Join-Path $Compiler "Share"

foreach ($dir in @($Units, $Programs, $Toolchain, $ToolLibraries, $Runtime, $ImportLibraries, $Include, $Packages, $Share)) {
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
    $mark = [regex]::Match($text, "\.(?:obj|o|rxm):\s*([\s\S]*)$")
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

function Get-UnitPath([string]$Source) {
    $relative = $Source.Substring($Root.Length + 1)
    $relative = [IO.Path]::ChangeExtension($relative, ".rxm")
    return Join-Path $Units $relative
}

function Move-LegacyUnit([string]$Source, [string]$Object) {
    if (Test-Path -LiteralPath $Object) { return }
    $stem = [IO.Path]::GetFileNameWithoutExtension($Source)
    $legacy = Join-Path $LegacyObj ($stem + ".obj")
    if (-not (Test-Path -LiteralPath $legacy)) { return }
    $parent = Split-Path -Parent $Object
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    Move-Item -LiteralPath $legacy -Destination $Object
    $legacyDep = [IO.Path]::ChangeExtension($legacy, ".d")
    $legacyFlags = [IO.Path]::ChangeExtension($legacy, ".flags")
    if (Test-Path -LiteralPath $legacyDep) {
        Move-Item -LiteralPath $legacyDep -Destination ([IO.Path]::ChangeExtension($Object, ".deps"))
    }
    if (Test-Path -LiteralPath $legacyFlags) {
        Move-Item -LiteralPath $legacyFlags -Destination ([IO.Path]::ChangeExtension($Object, ".stamp"))
    }
}

function Compile-Unit([string]$Source, [string]$Object) {
    Move-LegacyUnit $Source $Object
    $rel = $Source.Substring($Root.Length + 1)
    $depFile = [IO.Path]::ChangeExtension($Object, ".deps")
    $flagFile = [IO.Path]::ChangeExtension($Object, ".stamp")
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
    $parent = Split-Path -Parent $Object
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
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
Write-Host "Compiling $($sources.Count + $mainSources.Count) compiler units"
foreach ($file in @($sources + @(Get-Item -LiteralPath $mainSources))) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($file.Name)
    if ($usedNames.ContainsKey($stem)) {
        throw "Two compiler sources share the unit name $stem"
    }
    $usedNames[$stem] = $file.FullName
    $object = Get-UnitPath $file.FullName
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
    $mainObj = Get-UnitPath $source
    $exe = Join-Path $Programs $main.Output
    $linkInputs = @($objects + $mainObj)
    $linkCurrent = (Test-Path -LiteralPath $exe) -and -not (Test-AnyNewer $exe $linkInputs)
    if ($linkCurrent) {
        Write-Host "  skip link $($main.Output)"
        continue
    }
    Write-Host "Linking $($main.Output)"
    $importLib = Join-Path $ImportLibraries ([IO.Path]::GetFileNameWithoutExtension($main.Output) + ".lib")
    & $Clang -O2 @linkInputs -o $exe @linkLibs "-Wl,/IMPLIB:$importLib"
    if ($LASTEXITCODE -ne 0) {
        $linkFailed += $main.Output
        Write-Host "  $($main.Output) was not replaced. Close it if it is running, then run this build again."
    }
}
$runtimeUnits = @{
    "RobixNativeRuntime.rxm" = "Runtime\Native\RobixNativeRuntime.cpp"
    "LoopbackText.rxm" = "Runtime\Native\LoopbackText.cpp"
    "EnvStore.rxm" = "Runtime\Native\EnvStore.cpp"
    "NetworkRuntime.rxm" = "Network\NetworkRuntime.cpp"
}
foreach ($name in $runtimeUnits.Keys) {
    $from = Get-UnitPath (Join-Path $Root $runtimeUnits[$name])
    if (-not (Test-Path -LiteralPath $from)) { throw "missing runtime module $name" }
    $dest = Join-Path $Runtime $name
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
Copy-Tree (Join-Path $Root "RobixUI\Library") (Join-Path $Share "Schemas") "*.rxu"
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

$clangBin = Split-Path -Parent $Clang
$legacyToolchain = Join-Path $Prefix "Bin\toolchain"
$toolMap = @(
    @{ Name = "Clang.exe"; Origin = "clang.exe" },
    @{ Name = "Opt.exe"; Origin = "opt.exe" },
    @{ Name = "Linker.exe"; Origin = "ld.lld.exe" },
    @{ Name = "Archiver.exe"; Origin = "llvm-ar.exe" },
    @{ Name = "Symbols.exe"; Origin = "llvm-nm.exe" },
    @{ Name = "ObjectCopy.exe"; Origin = "llvm-objcopy.exe" }
)
foreach ($tool in $toolMap) {
    $dest = Join-Path $Toolchain $tool.Name
    $legacy = Join-Path $legacyToolchain $tool.Origin
    if ((Test-Path -LiteralPath $legacy) -and -not (Test-Path -LiteralPath $dest)) {
        Move-Item -LiteralPath $legacy -Destination $dest
        Write-Host "  moved $($tool.Name)"
        continue
    }
    Copy-ToolchainFile (Join-Path $clangBin $tool.Origin) $dest
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
$legacyLib = Join-Path $legacyToolchain "lib"
$libraryMap = @(
    @{ Name = "LibCmt.lib"; Origin = (Join-Path $msvcLib "libcmt.lib"); Legacy = "libcmt.lib" },
    @{ Name = "LibVcRuntime.lib"; Origin = (Join-Path $msvcLib "libvcruntime.lib"); Legacy = "libvcruntime.lib" },
    @{ Name = "LibCPP.lib"; Origin = (Join-Path $msvcLib "libcpmt.lib"); Legacy = "libcpmt.lib" },
    @{ Name = "OldNames.lib"; Origin = (Join-Path $msvcLib "oldnames.lib"); Legacy = "oldnames.lib" },
    @{ Name = "LibUcrt.lib"; Origin = (Join-Path $kitVersion.FullName "ucrt\x64\libucrt.lib"); Legacy = "libucrt.lib" },
    @{ Name = "Kernel32.lib"; Origin = (Join-Path $kitVersion.FullName "um\x64\kernel32.lib"); Legacy = "kernel32.lib" },
    @{ Name = "Ws232.lib"; Origin = (Join-Path $kitVersion.FullName "um\x64\ws2_32.lib"); Legacy = "ws2_32.lib" },
    @{ Name = "Secur32.lib"; Origin = (Join-Path $kitVersion.FullName "um\x64\secur32.lib"); Legacy = "secur32.lib" },
    @{ Name = "Crypt32.lib"; Origin = (Join-Path $kitVersion.FullName "um\x64\crypt32.lib"); Legacy = "crypt32.lib" }
)
foreach ($library in $libraryMap) {
    $dest = Join-Path $ToolLibraries $library.Name
    $legacy = Join-Path $legacyLib $library.Legacy
    if ((Test-Path -LiteralPath $legacy) -and -not (Test-Path -LiteralPath $dest)) {
        Move-Item -LiteralPath $legacy -Destination $dest
        Write-Host "  moved $($library.Name)"
        continue
    }
    Copy-ToolchainFile $library.Origin $dest
}
$builtins = Join-Path $clangBin "..\lib\clang\18\lib\windows\clang_rt.builtins-x86_64.lib"
$builtinsDest = Join-Path $ToolLibraries "ClangBuiltins.lib"
$legacyBuiltins = Join-Path $legacyLib "clang_rt.builtins-x86_64.lib"
if ((Test-Path -LiteralPath $legacyBuiltins) -and -not (Test-Path -LiteralPath $builtinsDest)) {
    Move-Item -LiteralPath $legacyBuiltins -Destination $builtinsDest
    Write-Host "  moved ClangBuiltins.lib"
} elseif (Test-Path -LiteralPath $builtins) {
    Copy-ToolchainFile $builtins $builtinsDest
}

Set-Content -LiteralPath (Join-Path $Share "Version.txt") -Value "Robix $Version`nRoboticela`n" -Encoding ascii
Set-Content -LiteralPath (Join-Path $Compiler "Toolchain.txt") -Value (Join-Path $Toolchain "Clang.exe") -Encoding ascii

Write-Host ""
Write-Host "Robix $Version"
Write-Host "  Compiler $Compiler"
Write-Host "  Toolchain $Toolchain"
Write-Host "  Runtime  $Runtime"
Write-Host "  Units    $Units"
Write-Host "  Include  $Include"
Write-Host "  Packages $Packages"
Write-Host "  Share    $Share"
Get-ChildItem -LiteralPath $Programs -File | ForEach-Object { Write-Host "  program  $($_.Name)" }
if ($linkFailed.Count -gt 0) {
    throw "link failed: $($linkFailed -join ', '). Close the running program and run this build again."
}
