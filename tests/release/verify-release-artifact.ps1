param(
    [string]$RepoRoot = "",
    [string]$Dist = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
}

if ([string]::IsNullOrWhiteSpace($Dist)) {
    $Dist = Join-Path $RepoRoot "dist"
}

function Get-SeedManifestPayloadFiles {
    param([Parameter(Mandatory = $true)][string]$RepoRoot)

    $manifestPath = Join-Path $RepoRoot "data\seeds\seed_manifest.json"
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "Seed manifest missing: $manifestPath"
    }

    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $files = New-Object System.Collections.Generic.List[string]
    foreach ($group in @($manifest.groups)) {
        foreach ($file in @($group.files)) {
            if ($null -eq $file.path -or [string]::IsNullOrWhiteSpace([string]$file.path)) {
                continue
            }
            $relativeValue = $file.source
            if ($null -eq $relativeValue -or [string]::IsNullOrWhiteSpace([string]$relativeValue)) {
                $relativeValue = $file.path
            }
            $relative = ([string]$relativeValue).Replace("/", "\")
            $files.Add((Join-Path "data\seeds" $relative))
        }
    }

    return $files | Sort-Object -Unique
}

if (-not (Test-Path -LiteralPath $Dist -PathType Container)) {
    throw "Release artifact directory does not exist: $Dist"
}

$requiredFiles = @(
    "KBOLauncher.exe",
    "KBOFix.dll",
    "WebView2Loader.dll",
    "kbo_league_id.txt",
    "assets\fonts\JejuGothic-Regular.ttf",
    "assets\fonts\JejuGothic-OFL.txt",
    "assets\icons\github-mark.png",
    "tools\kbo_optimizer.exe",
    "tools\kbo_optimizer.py",
    "tools\kbo_optimizer_lib\__init__.py",
    "tools\kbo_optimizer_lib\amateur_assignment.py",
    "tools\kbo_optimizer_lib\amateur_batch.py",
    "tools\kbo_optimizer_lib\amateur_common.py",
    "tools\kbo_optimizer_lib\amateur_metrics.py",
    "tools\kbo_optimizer_lib\amateur_role_capacities.py",
    "tools\kbo_optimizer_lib\amateur_roles.py",
    "tools\kbo_optimizer_lib\amateur_targets.py",
    "tools\kbo_optimizer_lib\asian_games_roster.py",
    "tools\kbo_optimizer_lib\cli.py",
    "tools\kbo_optimizer_lib\constants.py",
    "tools\kbo_optimizer_lib\csv_io.py",
    "tools\kbo_optimizer_lib\fa_compensation.py",
    "tools\kbo_optimizer_lib\military_selection.py"
)
$requiredFiles += Get-SeedManifestPayloadFiles -RepoRoot $RepoRoot

foreach ($requiredFile in $requiredFiles) {
    $path = Join-Path $Dist $requiredFile
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Release payload missing required file: $requiredFile"
    }
}

$manifest = Join-Path $RepoRoot "config\ootp-supported-builds.json"
$managedGenerated = Join-Path $RepoRoot "src\KBOLauncher\Infrastructure\OotpSupportedBuilds.Generated.cs"
$nativeGenerated = Join-Path $RepoRoot "native\src\build_verify\supported_builds.generated.c"
$nativeDll = Join-Path $RepoRoot "native\bin\KBOFix.dll"
$launcherExe = Join-Path $Dist "KBOLauncher.exe"

foreach ($path in @($manifest, $managedGenerated, $nativeGenerated, $nativeDll, $launcherExe)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Release validation input missing: $path"
    }
}

$manifestTime = (Get-Item -LiteralPath $manifest).LastWriteTimeUtc
foreach ($generated in @($managedGenerated, $nativeGenerated)) {
    if ((Get-Item -LiteralPath $generated).LastWriteTimeUtc -lt $manifestTime) {
        throw "Generated supported-build file is older than manifest: $generated"
    }
}

if ((Get-Item -LiteralPath $nativeDll).LastWriteTimeUtc -gt (Get-Item -LiteralPath $launcherExe).LastWriteTimeUtc) {
    throw "Native build appears newer than managed publish; run native build before publishing launcher."
}

$frameworkDependentFiles = @(
    "KBOLauncher.dll",
    "KBOLauncher.runtimeconfig.json",
    "KBOLauncher.deps.json"
)
foreach ($frameworkDependentFile in $frameworkDependentFiles) {
    $path = Join-Path $Dist $frameworkDependentFile
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        throw "Release payload looks framework-dependent; unexpected file present: $frameworkDependentFile"
    }
}

$leagueId = (Get-Content -LiteralPath (Join-Path $Dist "kbo_league_id.txt") -Raw).Trim()
if ($leagueId -ne "100") {
    throw "Release payload has unexpected kbo_league_id.txt value: '$leagueId'"
}

$fileCount = (Get-ChildItem -LiteralPath $Dist -Recurse -File).Count
Write-Host "Release artifact verified: $fileCount files in $Dist"
