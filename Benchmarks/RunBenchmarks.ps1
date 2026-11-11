<#
.SYNOPSIS
    Time the published Robix, C, and C++ kernels.

.DESCRIPTION
    Suites Micro, Numeric, and Systems are local kernels with a Robix, C, and C++
    source each. Flags are robixc -O3 and clang/clang++ -O3 -march=native.
    Two warmups, then five interleaved rounds. The highest sample is dropped.
    A result within 5 percent of the fastest time is a tie. After the timed pass
    the script waits, then runs one cool interleaved confirmation.

    An external suite name is refused unless that directory's manifest says
    upstream: true. The refusal text does not print the suite name.

.PARAMETER Suite
    Micro, Numeric, Systems, or All. Default is Micro.

.PARAMETER Runs
    Timed rounds after the warmups. Default is 5.

.PARAMETER CoolSeconds
    Pause before the confirmation pass. Default is 15.
#>

param(
    [string]$Suite = "Micro",
    [int]$Runs = 5,
    [int]$CoolSeconds = 15,
    [string]$BuildDir = "",
    [switch]$NoConfirm
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $repoRoot "Build\benchmarks"
}

$externalDirs = @{
    "spec"    = "SpecCPU2026"
    "dacapo"  = "DaCapoSuite"
    "npb"     = "NasParallelBenchmarks"
    "debian"  = "DebianBenchmarksGame"
}

function Read-Upstream([string]$dirName) {
    $path = Join-Path $repoRoot "Benchmarks\$dirName\manifest.txt"
    if (-not (Test-Path $path)) { return $false }
    $text = Get-Content $path -Raw
    return $text -match "(?im)^\s*upstream\s*:\s*true\s*$"
}

$asked = $Suite.Trim().ToLower()
if ($externalDirs.ContainsKey($asked)) {
    if (-not (Read-Upstream $externalDirs[$asked])) {
        Write-Host "refused: that suite's manifest does not set upstream: true."
        exit 2
    }
}

$published = @{
    "micro"   = "Micro"
    "numeric" = "Numeric"
    "systems" = "Systems"
}

$suitesToRun = @()
if ($asked -eq "all") {
    $suitesToRun = @("Micro", "Numeric", "Systems")
} elseif ($published.ContainsKey($asked)) {
    $suitesToRun = @($published[$asked])
} elseif ($externalDirs.ContainsKey($asked)) {
    Write-Host "refused: upstream is true but this runner has no published harness for that archive yet."
    exit 2
} else {
    Write-Host "usage: RunBenchmarks.ps1 -Suite Micro | Numeric | Systems | All"
    exit 2
}

$robixc  = Join-Path $repoRoot "Build\bin\robixc.exe"
$clang   = "C:\CLang\clang+llvm-18.1.8-x86_64-pc-windows-msvc\bin\clang.exe"
$clangpp = "C:\CLang\clang+llvm-18.1.8-x86_64-pc-windows-msvc\bin\clang++.exe"
foreach ($tool in @($robixc, $clang, $clangpp)) {
    if (-not (Test-Path $tool)) {
        Write-Host "missing tool: $tool"
        exit 1
    }
}
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

$robixcVersion  = ((& $robixc --version) | Select-Object -First 1)
$clangVersion   = ((& $clang --version) | Select-Object -First 1)
$clangppVersion = ((& $clangpp --version) | Select-Object -First 1)
$robixFlags = "robixc -O3"
$cFlags = "clang -O3 -march=native"
$cppFlags = "clang++ -O3 -march=native"

Write-Host "Robix benchmark runner"
Write-Host "robixc:  $robixcVersion"
Write-Host "clang:   $clangVersion"
Write-Host "clang++: $clangppVersion"
Write-Host "Flags:   $robixFlags / $cFlags / $cppFlags"
Write-Host "Suites:  $($suitesToRun -join ', ')"
Write-Host "Measure: 2 warmups, $Runs interleaved rounds, order rotated, highest sample dropped."
Write-Host "Tie:     within 5 percent of the fastest time."
Write-Host "These kernels are local. They are not an upstream archive."
Write-Host ""

function Get-ChecksumLine([string]$exePath) {
    $lines = @(& $exePath)
    $last = $lines | Where-Object { $_ -ne $null -and "$_".Trim() -ne "" } | Select-Object -Last 1
    if ($null -eq $last) { return "" }
    return "$last".Trim()
}

function Get-LastNumber([string]$line) {
    $found = [regex]::Matches($line, '-?\d+(?:\.\d+)?')
    if ($found.Count -eq 0) { return $null }
    return [double]$found[$found.Count - 1].Value
}

function Test-SameChecksum([string]$a, [string]$b, [string]$c) {
    $na = Get-LastNumber $a; $nb = Get-LastNumber $b; $nc = Get-LastNumber $c
    if ($null -eq $na -or $null -eq $nb -or $null -eq $nc) {
        return ($a -eq $b) -and ($b -eq $c)
    }
    return ($na -eq $nb) -and ($nb -eq $nc)
}

function Get-DroppedAverage($samples) {
    $sorted = @($samples | Sort-Object)
    $filtered = if ($sorted.Count -ge 4) { $sorted[0..($sorted.Count - 2)] } else { $sorted }
    return [Math]::Round((($filtered | Measure-Object -Average).Average), 2)
}

function Measure-Interleaved([string]$rxExe, [string]$cExe, [string]$cppExe, [int]$rounds, [switch]$SkipWarmup) {
    if (-not (Test-Path $rxExe) -or -not (Test-Path $cExe) -or -not (Test-Path $cppExe)) {
        return $null
    }
    $sumR = ""; $sumC = ""; $sumP = ""
    if (-not $SkipWarmup) {
        $sumR = Get-ChecksumLine $rxExe
        $sumC = Get-ChecksumLine $cExe
        $sumP = Get-ChecksumLine $cppExe
        & $rxExe | Out-Null
        & $cExe | Out-Null
        & $cppExe | Out-Null
    }
    $tr = @(); $tc = @(); $tp = @()
    for ($i = 0; $i -lt $rounds; $i++) {
        # Rotate which binary runs last. A fixed order gives the last process a warm cache.
        $slot = @()
        $slot += @{ Exe = $rxExe; Bag = "r" }
        $slot += @{ Exe = $cExe; Bag = "c" }
        $slot += @{ Exe = $cppExe; Bag = "p" }
        $start = $i % 3
        for ($k = 0; $k -lt 3; $k++) {
            $item = $slot[($start + $k) % 3]
            $sw = [System.Diagnostics.Stopwatch]::StartNew(); & $item.Exe | Out-Null; $sw.Stop()
            if ($item.Bag -eq "r") { $tr += $sw.Elapsed.TotalMilliseconds }
            elseif ($item.Bag -eq "c") { $tc += $sw.Elapsed.TotalMilliseconds }
            else { $tp += $sw.Elapsed.TotalMilliseconds }
        }
    }
    [PSCustomObject]@{
        RobixAvg = (Get-DroppedAverage $tr)
        CAvg     = (Get-DroppedAverage $tc)
        CPPAvg   = (Get-DroppedAverage $tp)
        SumRobix = $sumR
        SumC     = $sumC
        SumCPP   = $sumP
        ChecksumMatch = (Test-SameChecksum $sumR $sumC $sumP)
    }
}

function Get-Verdict($m) {
    if (-not $m.ChecksumMatch) { return "checksum mismatch" }
    $fast = [Math]::Min($m.RobixAvg, [Math]::Min($m.CAvg, $m.CPPAvg))
    if ($fast -le 0) { $fast = 0.01 }
    $band = $fast * 1.05
    $rClose = $m.RobixAvg -le $band
    $cClose = $m.CAvg -le $band
    $pClose = $m.CPPAvg -le $band
    $closeCount = 0
    if ($rClose) { $closeCount++ }
    if ($cClose) { $closeCount++ }
    if ($pClose) { $closeCount++ }
    if ($rClose -and $closeCount -ge 2) { return "tie" }
    if (-not $rClose) {
        if ($m.CAvg -le $m.CPPAvg) { return "loss to C" }
        return "loss to C++"
    }
    return "Robix"
}

$records = @()
$compileFailed = $false

foreach ($suiteName in $suitesToRun) {
    $sDir = Join-Path $repoRoot "Benchmarks\$suiteName"
    Write-Host "---- $suiteName ----"
    if (-not (Read-Upstream $suiteName)) {
        Write-Host "manifest upstream: false (local kernels)"
    }
    $rxFiles = Get-ChildItem -Path $sDir -Filter "*.rx" | Sort-Object Name
    foreach ($rf in $rxFiles) {
        $base = $rf.BaseName
        $cPath = Join-Path $sDir "$base.c"
        $cppPath = Join-Path $sDir "$base.cpp"
        if (-not (Test-Path $cPath) -or -not (Test-Path $cppPath)) { continue }
        $rxExe = Join-Path $BuildDir "${suiteName}_${base}_Robix.exe"
        $cExe   = Join-Path $BuildDir "${suiteName}_${base}_C.exe"
        $cppExe = Join-Path $BuildDir "${suiteName}_${base}_CPP.exe"
        Write-Host "compile $base"
        & $robixc $rf.FullName -o $rxExe -O3
        if ($LASTEXITCODE -ne 0) { $compileFailed = $true; continue }
        & $clang -O3 -march=native $cPath -o $cExe
        if ($LASTEXITCODE -ne 0) { $compileFailed = $true; continue }
        & $clangpp -O3 -march=native $cppPath -o $cppExe
        if ($LASTEXITCODE -ne 0) { $compileFailed = $true; continue }
        $hot = Measure-Interleaved $rxExe $cExe $cppExe $Runs
        $verdict = Get-Verdict $hot
        Write-Host ("{0}: Robix {1} ms, C {2} ms, C++ {3} ms, {4}" -f $base, $hot.RobixAvg, $hot.CAvg, $hot.CPPAvg, $verdict)
        Write-Host ("  checksum [{0}]" -f $hot.SumRobix)
        $records += [PSCustomObject]@{
            Suite = $suiteName; Name = $base; Hot = $hot; Verdict = $verdict; Cool = $null; Confirmed = ""
        }
    }
    Write-Host ""
}

if ($compileFailed) {
    Write-Host "compilation failed. No score is published from this run."
    exit 1
}

if (-not $NoConfirm -and $records.Count -gt 0) {
    Write-Host "cooling ${CoolSeconds}s before one interleaved confirmation"
    Start-Sleep -Seconds $CoolSeconds
    foreach ($rec in $records) {
        $rxExe = Join-Path $BuildDir "$($rec.Suite)_$($rec.Name)_Robix.exe"
        $cExe   = Join-Path $BuildDir "$($rec.Suite)_$($rec.Name)_C.exe"
        $cppExe = Join-Path $BuildDir "$($rec.Suite)_$($rec.Name)_CPP.exe"
        $cool = Measure-Interleaved $rxExe $cExe $cppExe 1 -SkipWarmup
        $cool.SumRobix = $rec.Hot.SumRobix
        $cool.SumC = $rec.Hot.SumC
        $cool.SumCPP = $rec.Hot.SumCPP
        $cool.ChecksumMatch = $rec.Hot.ChecksumMatch
        $coolVerdict = Get-Verdict $cool
        $rec.Cool = $cool
        $hotLoss = $rec.Verdict -like "loss to *"
        $coolLoss = $coolVerdict -like "loss to *"
        if ($rec.Verdict -eq "checksum mismatch" -or $coolVerdict -eq "checksum mismatch") {
            $rec.Confirmed = "checksum mismatch"
        } elseif ($rec.Verdict -eq "tie") {
            # One cool sample does not overturn a five-run tie.
            $rec.Confirmed = "tie"
        } elseif ($rec.Verdict -eq "Robix" -and $coolVerdict -eq "Robix") {
            $rec.Confirmed = "Robix"
        } elseif ($hotLoss -and $coolLoss) {
            $rec.Confirmed = "loss"
        } else {
            $rec.Confirmed = "unconfirmed"
        }
        Write-Host ("cool {0}: Robix {1} ms, C {2} ms, C++ {3} ms, hot {4}, cool {5}, published {6}" -f `
            $rec.Name, $cool.RobixAvg, $cool.CAvg, $cool.CPPAvg, $rec.Verdict, $coolVerdict, $rec.Confirmed)
    }
}

Write-Host ""
Write-Host "Published result (a five-run tie stands; a win or a loss counts only when the cool pass agrees on direction):"
$wins = 0; $ties = 0; $losses = 0; $other = 0
foreach ($rec in $records) {
    $label = if ($rec.Confirmed) { $rec.Confirmed } else { $rec.Verdict }
    Write-Host ("  {0}/{1}: {2}" -f $rec.Suite, $rec.Name, $label)
    if ($label -eq "Robix") { $wins++ }
    elseif ($label -eq "tie") { $ties++ }
    elseif ($label -eq "loss") { $losses++ }
    else { $other++ }
}
Write-Host ("Robix wins: {0}  ties: {1}  losses: {2}  other: {3}" -f $wins, $ties, $losses, $other)
Write-Host "A loss is left as a loss."
exit 0
