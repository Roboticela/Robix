$runs = 5

function Measure-Program($exePath, $name) {
    # Warmup
    & $exePath | Out-Null
    
    $times = @()
    for ($i = 0; $i -lt $runs; $i++) {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        & $exePath | Out-Null
        $sw.Stop()
        $times += $sw.Elapsed.TotalMilliseconds
    }
    
    $avg = ($times | Measure-Object -Average).Average
    $min = ($times | Measure-Object -Minimum).Minimum
    $max = ($times | Measure-Object -Maximum).Maximum
    
    [PSCustomObject]@{
        Name = $name
        AvgMs = [Math]::Round($avg, 2)
        MinMs = [Math]::Round($min, 2)
        MaxMs = [Math]::Round($max, 2)
    }
}

Write-Host "=========================================================="
Write-Host "          ROBIX NATIVE vs C PRODUCTION BENCHMARKS         "
Write-Host "=========================================================="
Write-Host "Platform: Windows x64 | Compiler Backend: LLVM/Clang 18.1.8 (-O2)"
Write-Host "Runs per benchmark: $runs (with initial warmup)"
Write-Host ""

$benchmarks = @(
    @{ Name = "Fibonacci & 100M Loop"; RobixExe = "Benchmarks\NativeVsC\FibRobix.exe"; CExe = "Benchmarks\NativeVsC\FibC.exe" },
    @{ Name = "Pi Leibniz 50M Floating-Point"; RobixExe = "Benchmarks\NativeVsC\PiRobix.exe"; CExe = "Benchmarks\NativeVsC\PiC.exe" },
    @{ Name = "Collatz Conjecture 2M Branching"; RobixExe = "Benchmarks\NativeVsC\CollatzRobix.exe"; CExe = "Benchmarks\NativeVsC\CollatzC.exe" }
)

$results = @()

foreach ($b in $benchmarks) {
    Write-Host "Benchmarking: $($b.Name)..."
    $cStats = Measure-Program $b.CExe "C (-O2)"
    $rbxStats = Measure-Program $b.RobixExe "Robix Native (-O2)"
    
    $ratio = [Math]::Round($rbxStats.AvgMs / $cStats.AvgMs, 3)
    $percentDiff = [Math]::Round((($rbxStats.AvgMs - $cStats.AvgMs) / $cStats.AvgMs) * 100.0, 2)
    
    $results += [PSCustomObject]@{
        Benchmark = $b.Name
        "C (Avg ms)" = $cStats.AvgMs
        "C (Min ms)" = $cStats.MinMs
        "Robix Native (Avg ms)" = $rbxStats.AvgMs
        "Robix Native (Min ms)" = $rbxStats.MinMs
        "Robix / C Ratio" = "$($ratio)x"
        "Diff (%)" = "$($percentDiff)%"
    }
}

Write-Host ""
$results | Format-Table -AutoSize

