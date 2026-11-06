<#
.SYNOPSIS
    Time local Robix, C, and C++ kernels. These are not upstream benchmark suites.

.DESCRIPTION
    Compiles .rbx / .c / .cpp triples from Benchmarks/ into the build directory and
    times them interleaved. Folder names DaCapoSuite, DebianBenchmarksGame,
    NasParallelBenchmarks, and SpecCpu2026 are local synthetic kernels.
    See Benchmarks/README.md. Upstream SPEC, DaCapo, NPB, and the Benchmarks Game
    are not vendored here.

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
    "DaCapo"    = @{ Dir = "DaCapoSuite";           Title = "Synthetic kernels in DaCapoSuite (not the DaCapo suite)"; Upstream = $false }
    "Debian"    = @{ Dir = "DebianBenchmarksGame";  Title = "Synthetic kernels in DebianBenchmarksGame (not the Benchmarks Game)"; Upstream = $false }
    "NPB"       = @{ Dir = "NasParallelBenchmarks"; Title = "Synthetic kernels in NasParallelBenchmarks (not NAS NPB)"; Upstream = $false }
    "SPEC"      = @{ Dir = "SpecCpu2026";           Title = "Synthetic kernels in SpecCpu2026 (not SPEC CPU)"; Upstream = $false }
    "NativeVsC" = @{ Dir = "NativeVsC";             Title = "Local Native vs C/C++ microbenchmarks"; Upstream = $false }
}

$suitesToRun = if ($Suite -eq "All") {
    @("DaCapo", "Debian", "NPB", "SPEC", "NativeVsC")
} else {
    @($Suite)
}

$robixcVersion = ((& $robixc --version) | Select-Object -First 1)
$clangVersion  = ((& $clang --version) | Select-Object -First 1)
$clangppVersion = ((& $clangpp --version) | Select-Object -First 1)
$robixFlags = "robixc -O3"
$cFlags = "clang -O3 -march=native"
$cppFlags = "clang++ -O3 -march=native"

Write-Host "==========================================================================================================" -ForegroundColor Cyan
Write-Host "              ROBIX vs C vs C++ -- LOCAL SYNTHETIC KERNELS                                              " -ForegroundColor Cyan
Write-Host "==========================================================================================================" -ForegroundColor Cyan
Write-Host "Upstream:        no. Not SPEC, DaCapo, NPB, or the Computer Language Benchmarks Game."
Write-Host "Platform:        Windows x64"
Write-Host "robixc:          $robixcVersion"
Write-Host "clang:           $clangVersion"
Write-Host "clang++:         $clangppVersion"
Write-Host ("Flags:           {0} / {1} / {2}" -f $robixFlags, $cFlags, $cppFlags)
Write-Host "Source Dir:      $benchRoot"
Write-Host "Build Output:    $BuildDir"
Write-Host "Suites to run:   $($suitesToRun -join ', ')"
Write-Host "Measurement:     2 warmups, then $Runs interleaved rounds (Robix, then C, then C++). Highest sample dropped when Runs >= 4."
Write-Host "Quote rule:      Keep this header, the checksum line, and the interleaved time together. A hot full sweep is not a cool confirmation."
Write-Host "==========================================================================================================`n"

function Get-ChecksumLine([string]$exePath) {
    $lines = @(& $exePath)
    $last = $lines | Where-Object { $_ -ne $null -and "$_".Trim() -ne "" } | Select-Object -Last 1
    if ($null -eq $last) { return "" }
    return "$last".Trim()
}

function Get-LastNumber([string]$line) {
    $matches = [regex]::Matches($line, '-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?')
    if ($matches.Count -eq 0) { return $null }
    return [double]$matches[$matches.Count - 1].Value
}

function Test-SameChecksum([string]$a, [string]$b, [string]$c) {
    $na = Get-LastNumber $a; $nb = Get-LastNumber $b; $nc = Get-LastNumber $c
    if ($null -eq $na -or $null -eq $nb -or $null -eq $nc) {
        return ($a -eq $b) -and ($b -eq $c)
    }
    $scale = [Math]::Max([Math]::Abs($na), 1.0)
    return ([Math]::Abs($na - $nb) -le (0.0001 * $scale)) -and ([Math]::Abs($nb - $nc) -le (0.0001 * $scale))
}

function Measure-Interleaved([string]$rbxExe, [string]$cExe, [string]$cppExe) {
    $missing = -not (Test-Path $rbxExe) -or -not (Test-Path $cExe) -or -not (Test-Path $cppExe)
    if ($missing) {
        return $null
    }
    $sumR = Get-ChecksumLine $rbxExe
    $sumC = Get-ChecksumLine $cExe
    $sumP = Get-ChecksumLine $cppExe
    & $rbxExe | Out-Null
    & $cExe | Out-Null
    & $cppExe | Out-Null

    $tr = @(); $tc = @(); $tp = @()
    for ($i = 0; $i -lt $Runs; $i++) {
        $sw = [System.Diagnostics.Stopwatch]::StartNew(); & $rbxExe | Out-Null; $sw.Stop(); $tr += $sw.Elapsed.TotalMilliseconds
        $sw = [System.Diagnostics.Stopwatch]::StartNew(); & $cExe | Out-Null; $sw.Stop(); $tc += $sw.Elapsed.TotalMilliseconds
        $sw = [System.Diagnostics.Stopwatch]::StartNew(); & $cppExe | Out-Null; $sw.Stop(); $tp += $sw.Elapsed.TotalMilliseconds
    }
    function AvgMin($samples) {
        $sorted = @($samples | Sort-Object)
        $filtered = if ($sorted.Count -ge 4) { $sorted[0..($sorted.Count - 2)] } else { $sorted }
        $avg = ($filtered | Measure-Object -Average).Average
        return @{ Avg = [Math]::Round($avg, 2); Min = [Math]::Round($sorted[0], 2) }
    }
    $r = AvgMin $tr; $c = AvgMin $tc; $p = AvgMin $tp
    $same = Test-SameChecksum $sumR $sumC $sumP
    [PSCustomObject]@{
        RobixAvg = $r.Avg; RobixMin = $r.Min
        CAvg = $c.Avg; CMin = $c.Min
        CPPAvg = $p.Avg; CPPMin = $p.Min
        SumRobix = $sumR; SumC = $sumC; SumCPP = $sumP
        ChecksumMatch = $same
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
    Write-Host "   Upstream suite: no" -ForegroundColor DarkGray
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

        $m = Measure-Interleaved $rbxExe $cExe $cppExe
        if ($null -eq $m) {
            Write-Host "  $($t.Base): binary missing, skipped"
            continue
        }

        $winner = "Robix"
        if (-not $m.ChecksumMatch) {
            $winner = "checksum mismatch"
        } elseif ($m.CAvg -lt $m.RobixAvg -and $m.CAvg -le $m.CPPAvg) {
            $winner = "C"
            $sCWins++
        } elseif ($m.CPPAvg -lt $m.RobixAvg -and $m.CPPAvg -le $m.CAvg) {
            $winner = "C++"
            $sCPPWins++
        } else {
            $sRobixWins++
        }

        $speedupC   = [Math]::Round((($m.CAvg - $m.RobixAvg) / $m.RobixAvg) * 100, 2)
        $speedupCPP = [Math]::Round((($m.CPPAvg - $m.RobixAvg) / $m.RobixAvg) * 100, 2)

        Write-Host ('  {0,-36}  Robix: {1,7} ms  C: {2,7} ms  C++: {3,7} ms  {4}' -f `
            $t.Base, $m.RobixAvg, $m.CAvg, $m.CPPAvg, $winner)
        Write-Host ('    checksum Robix [{0}]  C [{1}]  C++ [{2}]' -f $m.SumRobix, $m.SumC, $m.SumCPP)

        $suiteResults += [PSCustomObject]@{
            Benchmark  = $t.Base
            RobixAvg   = $m.RobixAvg
            RobixMin   = $m.RobixMin
            CAvg       = $m.CAvg
            CMin       = $m.CMin
            CPPAvg     = $m.CPPAvg
            CPPMin     = $m.CPPMin
            Winner     = $winner
            Checksum   = $m.SumRobix
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
    Write-Host "                         LOCAL SYNTHETIC KERNEL SCOREBOARD (not an upstream suite)                      " -ForegroundColor Green
    Write-Host "==========================================================================================================" -ForegroundColor Green
    Write-Host ("Total Benchmark Workloads: {0}" -f $totalTestsRun)
    Write-Host ("Robix Native Wins:         {0} / {1} ({2:P1})" -f $grandRobixWins, $totalTestsRun, ($grandRobixWins / [Math]::Max(1, $totalTestsRun)))
    Write-Host ("C (Clang 18) Wins:         {0} / {1} ({2:P1})" -f $grandCWins, $totalTestsRun, ($grandCWins / [Math]::Max(1, $totalTestsRun)))
    Write-Host ("C++ (Clang++ 18) Wins:     {0} / {1} ({2:P1})" -f $grandCPPWins, $totalTestsRun, ($grandCPPWins / [Math]::Max(1, $totalTestsRun)))
    Write-Host ("Flags: {0} / {1} / {2}" -f $robixFlags, $cFlags, $cppFlags)
    Write-Host "robixc: $robixcVersion"
    Write-Host "Interleaved rounds: $Runs after 2 warmups. Quote a row only with its checksum line."
    Write-Host "Compiled binaries stored at: $BuildDir (source directory remains 100% clean)."
    Write-Host "==========================================================================================================`n" -ForegroundColor Green
}

