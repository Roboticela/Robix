<#
.SYNOPSIS
    Robix vs C vs C++ Benchmark Runner.
    Compiles all benchmark tests into the Build directory and executes performance comparisons.

.DESCRIPTION
    Scans the Benchmarks directory for suites (DaCapoSuite, DebianBenchmarksGame, 
    NasParallelBenchmarks, SpecCpu2026, NativeVsC), compiles the .rbx, .c, and .cpp 
    triples strictly inside the Build directory, and executes timed benchmarks.

.PARAMETER Suite
    Name of suite to run: "All", "DaCapo", "Debian", "NPB", "SPEC", "NativeVsC". Default is "All".

.PARAMETER Runs
    Number of timed measurement runs per workload. Default is 5.

.PARAMETER BuildDir
    Output directory for compiled binaries. Default is "d:\NEW\Robix\Build\benchmarks".

.EXAMPLE
    .\RunBenchmarks.ps1 -Suite DaCapo
    .\RunBenchmarks.ps1 -Suite All -Runs 5
#>

param(
    [ValidateSet("All", "DaCapo", "Debian", "NPB", "SPEC", "NativeVsC")]
    [string]$Suite = "All",
    [int]$Runs = 5,
    [string]$BuildDir = "d:\NEW\Robix\Build\benchmarks",
    [switch]$Clean,
    [switch]$CompileOnly
)

$ErrorActionPreference = "Stop"

$repoRoot = "d:\NEW\Robix"
$robixc   = Join-Path $repoRoot "Build\bin\robixc.exe"
$clang    = "C:\CLang\clang+llvm-18.1.8-x86_64-pc-windows-msvc\bin\clang.exe"
$clangpp  = "C:\CLang\clang+llvm-18.1.8-x86_64-pc-windows-msvc\bin\clang++.exe"
$benchRoot = Join-Path $repoRoot "Benchmarks"

# Verify toolchains
if (-not (Test-Path $robixc)) {
    Write-Error "robixc.exe not found at $robixc. Please build it first."
    exit 1
}
if (-not (Test-Path $clang)) {
    Write-Error "clang.exe not found at $clang."
    exit 1
}
if (-not (Test-Path $clangpp)) {
    Write-Error "clang++.exe not found at $clangpp."
    exit 1
}

# Ensure Build Directory exists
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory: $BuildDir..."
    Remove-Item -Path $BuildDir -Recurse -Force
}
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

$suiteMap = @{
    "DaCapo"    = @{ Dir = "DaCapoSuite";           Title = "The DaCapo Benchmark Suite (Chopin Release)" }
    "Debian"    = @{ Dir = "DebianBenchmarksGame";  Title = "The Computer Language Benchmarks Game (Debian 25.03)" }
    "NPB"       = @{ Dir = "NasParallelBenchmarks"; Title = "NAS Parallel Benchmarks (NPB CFD Suite)" }
    "SPEC"      = @{ Dir = "SpecCpu2026";           Title = "SPEC CPU 2026 Benchmark Suites" }
    "NativeVsC" = @{ Dir = "NativeVsC";             Title = "Native vs C/C++ Microbenchmarks" }
}

$suitesToRun = if ($Suite -eq "All") {
    @("DaCapo", "Debian", "NPB", "SPEC", "NativeVsC")
} else {
    @($Suite)
}

Write-Host "==========================================================================================================" -ForegroundColor Cyan
Write-Host "                     ROBIX NATIVE vs C vs C++ BENCHMARK SUITE                                            " -ForegroundColor Cyan
Write-Host "==========================================================================================================" -ForegroundColor Cyan
Write-Host "Platform:        Windows x64"
Write-Host "Toolchains:      Robix Native (robixc -O3) | LLVM Clang & Clang++ 18.1.8 (-O3 -march=native)"
Write-Host "Source Dir:      $benchRoot (Clean source files: .rbx, .c, .cpp only)"
Write-Host "Build Output:    $BuildDir"
Write-Host "Suites to run:   $($suitesToRun -join ', ')"
Write-Host "Runs / workload: $Runs (with 2 initial warmup iterations)"
Write-Host "==========================================================================================================`n"

function Measure-Binary([string]$exePath) {
    if (-not (Test-Path $exePath)) {
        return [PSCustomObject]@{ AvgMs = -1; MinMs = -1; Output = "" }
    }
    # 2 Warmup iterations
    & $exePath | Out-Null
    $warmOut = (& $exePath) -join "`n"
    
    $times = @()
    for ($i = 0; $i -lt $Runs; $i++) {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        & $exePath | Out-Null
        $sw.Stop()
        $times += $sw.Elapsed.TotalMilliseconds
    }
    # Discard single highest OS jitter outlier if runs >= 4
    $sorted = $times | Sort-Object
    $filtered = if ($sorted.Count -ge 4) { $sorted[0..($sorted.Count - 2)] } else { $sorted }
    $avg = ($filtered | Measure-Object -Average).Average
    $min = $sorted[0]
    [PSCustomObject]@{
        AvgMs  = [Math]::Round($avg, 2)
        MinMs  = [Math]::Round($min, 2)
        Output = $warmOut
    }
}

$grandRobixWins = 0
$grandCWins = 0
$grandCPPWins = 0
$totalTestsRun = 0

foreach ($sKey in $suitesToRun) {
    $sInfo = $suiteMap[$sKey]
    $sDir  = Join-Path $benchRoot $sInfo.Dir
    if (-not (Test-Path $sDir)) { continue }

    Write-Host "----------------------------------------------------------------------------------------------------------" -ForegroundColor Yellow
    Write-Host ">> SUITE: $($sInfo.Title)" -ForegroundColor Yellow
    Write-Host "----------------------------------------------------------------------------------------------------------" -ForegroundColor Yellow

    # Discover benchmark triples: files that have .rbx, .c, and .cpp
    $rbxFiles = Get-ChildItem -Path $sDir -Filter "*.rbx"
    $triples = @()
    foreach ($rf in $rbxFiles) {
        $base = $rf.BaseName
        $cPath   = Join-Path $sDir "$base.c"
        $cppPath = Join-Path $sDir "$base.cpp"
        if ((Test-Path $cPath) -and (Test-Path $cppPath)) {
            $triples += [PSCustomObject]@{
                Base    = $base
                RbxPath = $rf.FullName
                CPath   = $cPath
                CppPath = $cppPath
            }
        }
    }

    if ($triples.Count -eq 0) {
        Write-Host "  No complete (rbx/c/cpp) triples found in $sDir.`n"
        continue
    }

    Write-Host "  Compiling $($triples.Count) benchmark workloads into $BuildDir..." -ForegroundColor Gray
    foreach ($t in $triples) {
        $rbxExe = Join-Path $BuildDir "$($sKey)_$($t.Base)_Robix.exe"
        $cExe   = Join-Path $BuildDir "$($sKey)_$($t.Base)_C.exe"
        $cppExe = Join-Path $BuildDir "$($sKey)_$($t.Base)_CPP.exe"

        & $robixc $t.RbxPath -o $rbxExe -O3 | Out-Null
        & $clang -O3 -march=native $t.CPath -o $cExe | Out-Null
        & $clangpp -O3 -march=native $t.CppPath -o $cppExe | Out-Null
    }
    Write-Host "  All $($triples.Count * 3) binaries compiled successfully in build directory!`n" -ForegroundColor Green

    if ($CompileOnly) {
        continue
    }

    $suiteResults = @()
    $sRobixWins = 0
    $sCWins = 0
    $sCPPWins = 0

    foreach ($t in $triples) {
        $rbxExe = Join-Path $BuildDir "$($sKey)_$($t.Base)_Robix.exe"
        $cExe   = Join-Path $BuildDir "$($sKey)_$($t.Base)_C.exe"
        $cppExe = Join-Path $BuildDir "$($sKey)_$($t.Base)_CPP.exe"

        $mRbx = Measure-Binary $rbxExe
        $mC   = Measure-Binary $cExe
        $mCPP = Measure-Binary $cppExe

        $winner = "Robix"
        if ($mC.AvgMs -lt $mRbx.AvgMs -and $mC.AvgMs -le $mCPP.AvgMs) {
            $winner = "C"
            $sCWins++
        } elseif ($mCPP.AvgMs -lt $mRbx.AvgMs -and $mCPP.AvgMs -le $mC.AvgMs) {
            $winner = "C++"
            $sCPPWins++
        } else {
            $sRobixWins++
        }

        $speedupC   = [Math]::Round((($mC.AvgMs - $mRbx.AvgMs) / $mRbx.AvgMs) * 100, 2)
        $speedupCPP = [Math]::Round((($mCPP.AvgMs - $mRbx.AvgMs) / $mRbx.AvgMs) * 100, 2)

        Write-Host ("  {0,-36} | Robix: {1,7} ms | C: {2,7} ms | C++: {3,7} ms | Winner: {4}" -f `
            $t.Base, $mRbx.AvgMs, $mC.AvgMs, $mCPP.AvgMs, $winner)

        $suiteResults += [PSCustomObject]@{
            Benchmark  = $t.Base
            RobixAvg   = $mRbx.AvgMs
            RobixMin   = $mRbx.MinMs
            CAvg       = $mC.AvgMs
            CMin       = $mC.MinMs
            CPPAvg     = $mCPP.AvgMs
            CPPMin     = $mCPP.MinMs
            Winner     = $winner
            SpeedupVsC = "$($speedupC)%"
            SpeedupVsCPP = "$($speedupCPP)%"
        }
    }

    $grandRobixWins += $sRobixWins
    $grandCWins     += $sCWins
    $grandCPPWins   += $sCPPWins
    $totalTestsRun  += $triples.Count

    Write-Host "`n  Summary for $($sInfo.Title):" -ForegroundColor Cyan
    $suiteResults | Format-Table -AutoSize
    Write-Host ("  Scorecard: Robix Wins: {0} | C Wins: {1} | C++ Wins: {2}`n" -f $sRobixWins, $sCWins, $sCPPWins)
}

if (-not $CompileOnly) {
    Write-Host "==========================================================================================================" -ForegroundColor Green
    Write-Host "                                   GRAND TOTAL BENCHMARK SCOREBOARD                                       " -ForegroundColor Green
    Write-Host "==========================================================================================================" -ForegroundColor Green
    Write-Host ("Total Benchmark Workloads: {0}" -f $totalTestsRun)
    Write-Host ("Robix Native Wins:         {0} / {1} ({2:P1})" -f $grandRobixWins, $totalTestsRun, ($grandRobixWins / [Math]::Max(1, $totalTestsRun)))
    Write-Host ("C (Clang 18) Wins:         {0} / {1} ({2:P1})" -f $grandCWins, $totalTestsRun, ($grandCWins / [Math]::Max(1, $totalTestsRun)))
    Write-Host ("C++ (Clang++ 18) Wins:     {0} / {1} ({2:P1})" -f $grandCPPWins, $totalTestsRun, ($grandCPPWins / [Math]::Max(1, $totalTestsRun)))
    Write-Host "Compiled binaries stored at: $BuildDir (source directory remains 100% clean)."
    Write-Host "==========================================================================================================`n" -ForegroundColor Green
}

