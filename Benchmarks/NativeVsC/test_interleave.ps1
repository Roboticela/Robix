$cTimes = @()
$rbxTimes = @()
for ($i = 0; $i -lt 5; $i++) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & Benchmarks\NativeVsC\CollatzRobixMax.exe | Out-Null
    $sw.Stop()
    $rbxTimes += $sw.Elapsed.TotalMilliseconds

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & Benchmarks\NativeVsC\CollatzCMax.exe | Out-Null
    $sw.Stop()
    $cTimes += $sw.Elapsed.TotalMilliseconds
}
Write-Host "Robix First - Robix Avg: $([Math]::Round(($rbxTimes | Measure-Object -Average).Average, 2)) ms"
Write-Host "Robix First - C Avg:     $([Math]::Round(($cTimes | Measure-Object -Average).Average, 2)) ms"

