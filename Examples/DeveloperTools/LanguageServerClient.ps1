$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = (Resolve-Path ".\Build\bin\robix.exe").Path
$psi.Arguments = "lsp"
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$proc = [Diagnostics.Process]::Start($psi)

function Send-Lsp([object]$message) {
    $json = $message | ConvertTo-Json -Compress -Depth 8
    $bytes = [Text.Encoding]::UTF8.GetBytes($json)
    $header = [Text.Encoding]::ASCII.GetBytes("Content-Length: $($bytes.Length)`r`n`r`n")
    $proc.StandardInput.BaseStream.Write($header, 0, $header.Length)
    $proc.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length)
    $proc.StandardInput.BaseStream.Flush()
}

function Read-Lsp {
    $length = 0
    while ($true) {
        $line = $proc.StandardOutput.ReadLine()
        if ($null -eq $line) { return "" }
        if ($line -match "Content-Length:\s*(\d+)") { $length = [int]$Matches[1] }
        if ($line -eq "") { break }
    }
    $buffer = New-Object char[] $length
    $read = 0
    while ($read -lt $length) {
        $count = $proc.StandardOutput.Read($buffer, $read, $length - $read)
        if ($count -le 0) { break }
        $read += $count
    }
    return -join $buffer
}

$source = [IO.File]::ReadAllText((Resolve-Path ".\Examples\DeveloperTools\Documented.rx"))
Send-Lsp @{ jsonrpc = "2.0"; id = 1; method = "initialize"; params = @{} }
$init = Read-Lsp
Send-Lsp @{
    jsonrpc = "2.0"
    method = "textDocument/didOpen"
    params = @{
        textDocument = @{
            uri = "file:///Examples/DeveloperTools/Documented.rx"
            languageId = "robix"
            version = 1
            text = $source
        }
    }
}
Send-Lsp @{
    jsonrpc = "2.0"
    id = 2
    method = "textDocument/hover"
    params = @{
        textDocument = @{ uri = "file:///Examples/DeveloperTools/Documented.rx" }
        position = @{ line = 18; character = 20 }
    }
}
$hover = Read-Lsp
Send-Lsp @{ jsonrpc = "2.0"; id = 3; method = "shutdown"; params = $null }
$shutdown = Read-Lsp
Send-Lsp @{ jsonrpc = "2.0"; method = "exit" }
Write-Output "initialize: $init"
Write-Output "hover: $hover"
Write-Output "shutdown: $shutdown"
if (-not $proc.WaitForExit(5000)) { $proc.Kill() }
exit $proc.ExitCode
