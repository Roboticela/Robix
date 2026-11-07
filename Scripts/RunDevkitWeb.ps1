param(
    [switch]$Build,
    [int]$Port = 4173
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Web = Join-Path $Root "Build\web\DevKit"
$Index = Join-Path $Web "index.html"
$Document = Join-Path $Root "DevKit\Main.xml"
$Robix = Join-Path $Root "Build\bin\robix-ui.exe"
if (-not (Test-Path -LiteralPath $Robix)) {
    $Robix = Join-Path $Root "Build\bin\robix.exe"
}

function Test-DevKitWebStale {
    if (-not (Test-Path -LiteralPath $Robix) -or -not (Test-Path -LiteralPath $Index)) { return $true }
    $indexTime = (Get-Item -LiteralPath $Index).LastWriteTimeUtc
    $inputs = @(Get-Item -LiteralPath $Robix)
    $inputs += @(Get-ChildItem (Join-Path $Root "DevKit") -Recurse -File | Where-Object {
            $_.Extension -in ".xml", ".cpp", ".hpp", ".rbxu"
        })
    foreach ($file in $inputs) {
        if ($file.LastWriteTimeUtc -gt $indexTime) { return $true }
    }
    return $false
}

function Update-DevKitWeb {
    if (-not (Test-Path -LiteralPath $Robix)) {
        throw "robix-ui.exe is missing. Build the Robix tools before running DevKit."
    }
    Write-Host "Compiling DevKit for Web..."
    & $Robix ui web $Document -o $Web
    if ($LASTEXITCODE -ne 0) { throw "DevKit web compile failed" }
}

if ($Build -or (Test-DevKitWebStale)) {
    Update-DevKitWeb
}
if (-not (Test-Path -LiteralPath $Index)) {
    throw "DevKit web package missing: $Index"
}

$Prefix = "http://127.0.0.1:$Port/"
$Listener = New-Object System.Net.HttpListener
$Listener.Prefixes.Add($Prefix)
try {
    $Listener.Start()
} catch {
    throw "Could not listen on $Prefix. Close anything already using that port, or pass -Port."
}

$Types = @{
    ".html" = "text/html; charset=utf-8"
    ".css"  = "text/css; charset=utf-8"
    ".js"   = "text/javascript; charset=utf-8"
    ".mp4"  = "video/mp4"
    ".ttf"  = "font/ttf"
    ".svg"  = "image/svg+xml"
    ".png"  = "image/png"
    ".jpg"  = "image/jpeg"
    ".jpeg" = "image/jpeg"
    ".webp" = "image/webp"
    ".json" = "application/json"
}

Write-Host "DevKit web at $Prefix"
Write-Host "Press Ctrl+C to stop."
Start-Process $Prefix

try {
    while ($Listener.IsListening) {
        $Context = $Listener.GetContext()
        $Request = $Context.Request
        $Response = $Context.Response
        $Relative = [Uri]::UnescapeDataString($Request.Url.AbsolutePath.TrimStart("/"))
        if ([string]::IsNullOrWhiteSpace($Relative)) { $Relative = "index.html" }
        $Relative = $Relative.Replace("/", [IO.Path]::DirectorySeparatorChar)
        $Full = [IO.Path]::GetFullPath((Join-Path $Web $Relative))
        $WebFull = [IO.Path]::GetFullPath($Web)
        if (-not $Full.StartsWith($WebFull, [StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $Full -PathType Leaf)) {
            $Response.StatusCode = 404
            $Missing = [Text.Encoding]::UTF8.GetBytes("Not found")
            $Response.OutputStream.Write($Missing, 0, $Missing.Length)
            $Response.Close()
            continue
        }
        $Bytes = [IO.File]::ReadAllBytes($Full)
        $Extension = [IO.Path]::GetExtension($Full).ToLowerInvariant()
        if ($Types.ContainsKey($Extension)) {
            $Response.ContentType = $Types[$Extension]
        } else {
            $Response.ContentType = "application/octet-stream"
        }
        $Response.Headers["Accept-Ranges"] = "bytes"
        $Start = 0
        $End = $Bytes.Length - 1
        $Range = $Request.Headers["Range"]
        if ($Range -and $Range.StartsWith("bytes=")) {
            $Span = $Range.Substring(6).Split("-")
            if ($Span[0]) { $Start = [int]$Span[0] }
            if ($Span.Length -gt 1 -and $Span[1]) { $End = [int]$Span[1] }
            if ($Start -lt 0) { $Start = 0 }
            if ($End -ge $Bytes.Length) { $End = $Bytes.Length - 1 }
            $Response.StatusCode = 206
            $Response.Headers["Content-Range"] = "bytes $Start-$End/$($Bytes.Length)"
        }
        $Length = $End - $Start + 1
        $Response.ContentLength64 = $Length
        $Response.OutputStream.Write($Bytes, $Start, $Length)
        $Response.Close()
    }
} finally {
    if ($Listener.IsListening) { $Listener.Stop() }
    $Listener.Close()
}
