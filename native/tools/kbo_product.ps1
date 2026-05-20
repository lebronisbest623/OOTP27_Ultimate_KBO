$script:KboProductLocalDataDirectoryName = "OOTP-KBO"
$script:KboProductSaveScopedDirectoryName = "saves"
$script:KboProductPerfDirectoryName = "perf"
$script:KboProductPerfFilePattern = "kbo_perf_*.csv"
$script:KboPerfSnapshotPlayerFileName = "perf_snapshot_players.bin"

function Get-KboLocalDataPath {
    param(
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]] $Parts
    )

    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        throw "LOCALAPPDATA is not set"
    }

    $path = Join-Path $env:LOCALAPPDATA $script:KboProductLocalDataDirectoryName
    foreach ($part in $Parts) {
        if ([string]::IsNullOrWhiteSpace($part)) {
            continue
        }
        $path = Join-Path $path $part
    }
    return $path
}
