$runs = 10

function Measure-Exe($path) {
    & $path | Out-Null # warmup
    $times = @()
    for ($i = 0; $i -lt $runs; $i++) {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        & $path | Out-Null
        $sw.Stop()
        $times += $sw.Elapsed.TotalMilliseconds
    }
    return [PSCustomObject]@{
        Min = [Math]::Round(($times | Measure-Object -Minimum).Minimum, 2)
        Avg = [Math]::Round(($times | Measure-Object -Average).Average, 2)
    }
}

$cRes = Measure-Exe "Benchmarks\NativeVsC\CollatzCMax.exe"
$rbxRes = Measure-Exe "Benchmarks\NativeVsC\CollatzRobixMax.exe"

Write-Host "Results over $runs runs:"
Write-Host "C Native Max:     Min = $($cRes.Min) ms | Avg = $($cRes.Avg) ms"
Write-Host "Robix Native Max: Min = $($rbxRes.Min) ms | Avg = $($rbxRes.Avg) ms"
$ratio = [Math]::Round($rbxRes.Avg / $cRes.Avg, 3)
Write-Host "Robix / C Ratio: $($ratio)x"

