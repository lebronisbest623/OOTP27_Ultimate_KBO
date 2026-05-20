#Requires -Version 7.0
param(
    [int] $Samples = 60,
    [int] $Players = 5000,
    [string] $SnapshotPath = "",
    [switch] $AnalyzePerf,
    [switch] $ReplayPerf,
    [string] $ReplayPerfPath = "",
    [string] $ReplayPlanPath = "",
    [string] $MapPath = "",
    [int] $AnalyzeTop = 25,
    [int] $AnalyzeUnmappedTop = 25,
    [int] $AnalyzeGroupDepth = 2,
    [ValidateSet("fast", "virtual", "virtualquery")]
    [string] $Readability = "fast",
    [switch] $BuildOnly
)

$ErrorActionPreference = "Stop"

$BenchDir = $PSScriptRoot
$Root = Split-Path -Parent $BenchDir
$RepoRoot = Split-Path -Parent $Root
. (Join-Path $Root "tools\kbo_product.ps1")
$BenchSrc = Join-Path $BenchDir "bench_main.c"
$BenchExe = Join-Path $BenchDir "bench.exe"
$ResultsDir = Join-Path $BenchDir "results"
$AnalyzeScript = Join-Path $RepoRoot "tools\analyze-perf-overhead.ps1"

function Resolve-Gcc {
    $Candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($env:KBO_GCC)) {
        $Candidates += $env:KBO_GCC
    }

    $WingetPackages = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages"
    if (Test-Path -LiteralPath $WingetPackages) {
        $Candidates += Get-ChildItem -LiteralPath $WingetPackages -Recurse -Filter "gcc.exe" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match "\\mingw(32|64)\\bin\\gcc\.exe$" } |
            Sort-Object FullName -Descending |
            ForEach-Object { $_.FullName }
    }

    $Candidates += @(
        "C:\msys64\ucrt64\bin\gcc.exe",
        "C:\msys64\mingw64\bin\gcc.exe",
        "C:\Program Files\mingw64\bin\gcc.exe",
        "C:\mingw64\bin\gcc.exe",
        "gcc.exe"
    )

    foreach ($Candidate in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($Candidate)) {
            continue
        }

        $Command = Get-Command $Candidate -ErrorAction SilentlyContinue
        if ($Command) {
            return $Command.Source
        }

        if (Test-Path -LiteralPath $Candidate) {
            return (Resolve-Path -LiteralPath $Candidate).Path
        }
    }

    throw "Could not find gcc.exe. Install MinGW-w64 GCC, add gcc.exe to PATH, or set KBO_GCC to the full gcc.exe path."
}

New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null

function Resolve-SnapshotPath {
    param([string] $Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }
    if ($Path -ieq "latest") {
        $SaveScopedRoot = Get-KboLocalDataPath $script:KboProductSaveScopedDirectoryName
        if (-not (Test-Path -LiteralPath $SaveScopedRoot)) {
            throw "No save-scoped data directory found at $SaveScopedRoot"
        }
        $Latest = Get-ChildItem -LiteralPath $SaveScopedRoot -Recurse -Filter $script:KboPerfSnapshotPlayerFileName -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if (-not $Latest) {
            throw "No $($script:KboPerfSnapshotPlayerFileName) found below $SaveScopedRoot"
        }
        return $Latest.FullName
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Snapshot path not found: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Resolve-PerfPath {
    param([string] $Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }
    if ($Path -ieq "latest") {
        $PerfRoot = Get-KboLocalDataPath $script:KboProductPerfDirectoryName
        if (-not (Test-Path -LiteralPath $PerfRoot)) {
            throw "No profiler directory found at $PerfRoot"
        }
        $Latest = Get-ChildItem -LiteralPath $PerfRoot -Filter $script:KboProductPerfFilePattern -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if (-not $Latest) {
            throw "No $($script:KboProductPerfFilePattern) found below $PerfRoot"
        }
        return $Latest.FullName
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Profiler CSV not found: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

$Gcc = Resolve-Gcc
Write-Host "GCC: $Gcc"

& $Gcc -O2 -DNDEBUG -DKBO_BENCHMARK_BUILD -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $BenchExe `
    $BenchSrc `
    (Join-Path $Root "src\core\csv\core_csv.c") `
    (Join-Path $Root "src\core\dates\core_text_date.c") `
    (Join-Path $Root "src\foreign\common\policy\foreign_player_policy.c") `
    (Join-Path $Root "src\foreign\common\player_eval\foreign_waiver_player_eval.c") `
    (Join-Path $Root "src\foreign\quota\counts\org\foreign_quota_count_state.c") `
    (Join-Path $Root "src\foreign\quota\counts\org\foreign_quota_count_cache.c") `
    (Join-Path $Root "src\foreign\quota\counts\org\foreign_quota_count_snapshot.c") `
    (Join-Path $Root "src\foreign\quota\counts\foreign_quota_counts.c") `
    (Join-Path $Root "src\foreign\quota\candidates\cache\foreign_quota_candidate_limit_cache.c") `
    (Join-Path $Root "src\foreign\quota\candidates\foreign_quota_candidate_limits.c") `
    (Join-Path $Root "src\foreign\signability\foreign_policy\policy\foreign_signability_foreign_fa_fast_block_policy.c") `
    (Join-Path $Root "src\fa_compensation\protection\cache\fa_compensation_protection_cache.c") `
    (Join-Path $Root "src\fa_compensation\protection\policy\fa_compensation_protection_policy.c") `
    (Join-Path $Root "src\fa_compensation\protection\candidate_score\fa_compensation_candidate_score.c") `
    (Join-Path $Root "src\fa_compensation\protection\fa_compensation_protection_score.c")
if ($LASTEXITCODE -ne 0) {
    throw "Benchmark build failed"
}

if ($BuildOnly) {
    Write-Host "Built $BenchExe"
    return
}

$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$UseReplay = $ReplayPerf -or -not [string]::IsNullOrWhiteSpace($ReplayPerfPath)
$ResultPrefix = if ($UseReplay) { "kbo_bench_replay" } else { "kbo_bench" }
$ResultPath = Join-Path $ResultsDir "$ResultPrefix`_$Stamp.csv"

$ResolvedSnapshotPath = Resolve-SnapshotPath -Path $SnapshotPath
$ResolvedMapPath = $MapPath
if ([string]::IsNullOrWhiteSpace($ResolvedMapPath)) {
    $ResolvedMapPath = Join-Path $RepoRoot "tools\perf_overhead_map.sample.json"
}

$ResolvedReplayPerfPath = ""
if ($UseReplay) {
    if ([string]::IsNullOrWhiteSpace($ReplayPerfPath)) {
        $ReplayPerfPath = "latest"
    }
    $ResolvedReplayPerfPath = Resolve-PerfPath -Path $ReplayPerfPath

    if (-not (Test-Path -LiteralPath $AnalyzeScript)) {
        throw "Perf analyzer not found: $AnalyzeScript"
    }

    $ResolvedReplayPlanPath = $ReplayPlanPath
    if ([string]::IsNullOrWhiteSpace($ResolvedReplayPlanPath)) {
        $ResolvedReplayPlanPath = Join-Path $ResultsDir "kbo_bench_replay_plan_$Stamp.csv"
    }

    $PlanArgs = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $AnalyzeScript,
        "-PerfPath", $ResolvedReplayPerfPath,
        "-MapPath", $ResolvedMapPath,
        "-WriteReplayPlan", $ResolvedReplayPlanPath,
        "-PlanOnly"
    )

    & pwsh @PlanArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Replay plan generation failed"
    }
}

$BenchArgs = @("--samples", "$Samples", "--players", "$Players", "--readability", $Readability)
if (-not [string]::IsNullOrWhiteSpace($ResolvedSnapshotPath)) {
    Write-Host "Snapshot: $ResolvedSnapshotPath"
    $BenchArgs += @("--snapshot", $ResolvedSnapshotPath)
}
if ($UseReplay) {
    Write-Host "Replay perf: $ResolvedReplayPerfPath"
    Write-Host "Replay plan: $ResolvedReplayPlanPath"
    $BenchArgs += @("--plan", $ResolvedReplayPlanPath)
}

$ResultPathParent = Split-Path -Parent $ResultPath
New-Item -ItemType Directory -Force -Path $ResultPathParent | Out-Null
& $BenchExe @BenchArgs | Tee-Object -FilePath $ResultPath
$BenchExitCode = $LASTEXITCODE
if ($BenchExitCode -ne 0) {
    throw "Benchmark run failed"
}
Write-Host "Wrote $ResultPath"

if ($UseReplay) {
    $Rows = Import-Csv -LiteralPath $ResultPath
    $TotalMs = ($Rows | Measure-Object -Property total_ms -Sum).Sum
    $TotalIterations = ($Rows | Measure-Object -Property iterations -Sum).Sum
    Write-Host ("Replay measured iterations: {0:N0}" -f $TotalIterations)
    Write-Host ("Replay measured total: {0:N3} ms ({1:N3} s)" -f $TotalMs, ($TotalMs / 1000.0))
}

if ($AnalyzePerf -or $UseReplay) {
    if (-not (Test-Path -LiteralPath $AnalyzeScript)) {
        throw "Perf analyzer not found: $AnalyzeScript"
    }

    $AnalyzeArgs = @(
        "-ExecutionPolicy", "Bypass",
        "-File", $AnalyzeScript,
        "-BenchPath", $ResultPath,
        "-MapPath", $ResolvedMapPath,
        "-Top", "$AnalyzeTop",
        "-UnmappedTop", "$AnalyzeUnmappedTop",
        "-GroupDepth", "$AnalyzeGroupDepth"
    )
    if (-not [string]::IsNullOrWhiteSpace($ResolvedReplayPerfPath)) {
        $AnalyzeArgs += @("-PerfPath", $ResolvedReplayPerfPath)
    }

    & pwsh @AnalyzeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Perf coverage analysis failed"
    }
}
