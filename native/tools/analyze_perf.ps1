param(
    [string]$Path,
    [int]$Top = 25
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "kbo_product.ps1")

if ([string]::IsNullOrWhiteSpace($Path)) {
    $perfDir = Get-KboLocalDataPath $script:KboProductPerfDirectoryName
    if (-not (Test-Path -LiteralPath $perfDir)) {
        throw "Perf directory not found: $perfDir"
    }
    $latest = Get-ChildItem -LiteralPath $perfDir -Filter $script:KboProductPerfFilePattern |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $latest) {
        throw "No profiler CSV found in $perfDir"
    }
    $Path = $latest.FullName
}

$rows = Import-Csv -LiteralPath $Path
if (-not $rows) {
    throw "No rows in profiler CSV: $Path"
}

$summary = $rows |
    Group-Object zone |
    ForEach-Object {
        $deltaCalls = 0L
        $deltaUs = 0L
        $maxUs = 0L
        $slow = 0L
        foreach ($row in $_.Group) {
            $deltaCalls += [int64]$row.delta_calls
            $deltaUs += [int64]$row.delta_us
            $candidateMax = [int64]$row.max_us
            if ($candidateMax -gt $maxUs) {
                $maxUs = $candidateMax
            }
            $slow += [int64]$row.delta_slow_calls
        }
        [pscustomobject]@{
            Zone = $_.Name
            Calls = $deltaCalls
            TotalMs = [math]::Round($deltaUs / 1000.0, 3)
            AvgUs = if ($deltaCalls -gt 0) { [math]::Round($deltaUs / [double]$deltaCalls, 2) } else { 0 }
            MaxUs = $maxUs
            SlowCalls = $slow
        }
    }

Write-Host "Profiler CSV: $Path"
Write-Host ""
Write-Host "Top by total time"
$summary | Sort-Object TotalMs -Descending | Select-Object -First $Top | Format-Table -AutoSize

Write-Host ""
Write-Host "Top by call count"
$summary | Sort-Object Calls -Descending | Select-Object -First $Top | Format-Table -AutoSize

Write-Host ""
Write-Host "Top by max latency"
$summary | Sort-Object MaxUs -Descending | Select-Object -First $Top | Format-Table -AutoSize
