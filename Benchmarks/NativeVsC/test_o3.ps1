$cTimes = @()
$rbxTimes = @()
for ($i = 0; $i -lt 5; $i++) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & Benchmarks\NativeVsC\CollatzCO3.exe | Out-Null
    $sw.Stop()
    $cTimes += $sw.Elapsed.TotalMilliseconds

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & Benchmarks\NativeVsC\CollatzRobixO3.exe | Out-Null
    $sw.Stop()
    $rbxTimes += $sw.Elapsed.TotalMilliseconds
}
Write-Host "C O3 Avg: $([Math]::Round(($cTimes | Measure-Object -Average).Average, 2)) ms"
Write-Host "Robix O3 Avg: $([Math]::Round(($rbxTimes | Measure-Object -Average).Average, 2)) ms"

