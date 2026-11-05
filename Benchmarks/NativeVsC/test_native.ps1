$cTimes = @()
$rbxTimes = @()
for ($i = 0; $i -lt 7; $i++) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & Benchmarks\NativeVsC\CollatzCNative.exe | Out-Null
    $sw.Stop()
    $cTimes += $sw.Elapsed.TotalMilliseconds

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & Benchmarks\NativeVsC\CollatzRobixNative.exe | Out-Null
    $sw.Stop()
    $rbxTimes += $sw.Elapsed.TotalMilliseconds
}
Write-Host "C Native march=native Min: $([Math]::Round(($cTimes | Measure-Object -Minimum).Minimum, 2)) ms, Avg: $([Math]::Round(($cTimes | Measure-Object -Average).Average, 2)) ms"
Write-Host "Robix Native march=native Min: $([Math]::Round(($rbxTimes | Measure-Object -Minimum).Minimum, 2)) ms, Avg: $([Math]::Round(($rbxTimes | Measure-Object -Average).Average, 2)) ms"

