param(
    [switch]$RebuildOptimizer
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Dist = Join-Path $RepoRoot "dist"

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

if (Test-Path -LiteralPath $Dist) {
    Remove-Item -LiteralPath $Dist -Recurse -Force
}
New-Item -ItemType Directory -Path $Dist | Out-Null

Write-Host "==> Building native DLL..."
& pwsh -ExecutionPolicy Bypass -File (Join-Path $RepoRoot "native\build.ps1")
if ($LASTEXITCODE -ne 0) { throw "Native build failed" }

Write-Host "==> Building optimizer tool..."
$OptimizerSource = Join-Path $RepoRoot "tools\kbo_optimizer.py"
$OptimizerLibSource = Join-Path $RepoRoot "tools\kbo_optimizer_lib"
$PyInstallerWork = Join-Path $RepoRoot "obj\pyinstaller"
$OptimizerExe = Join-Path $RepoRoot "tools\kbo_optimizer.exe"

$OptimizerNeedsBuild = $RebuildOptimizer `
    -or -not (Test-Path -LiteralPath $OptimizerExe -PathType Leaf) `
    -or ((Get-Item -LiteralPath $OptimizerExe).LastWriteTimeUtc -lt (Get-Item -LiteralPath $OptimizerSource).LastWriteTimeUtc)

if ($OptimizerNeedsBuild) {
    if (Test-Path -LiteralPath $OptimizerExe) {
        Remove-Item -LiteralPath $OptimizerExe -Force
    }
    New-Item -ItemType Directory -Path $PyInstallerWork -Force | Out-Null
    $PyInstallerExcludes = @(
        "boto3",
        "botocore",
        "fsspec",
        "IPython",
        "llvmlite",
        "lxml",
        "PyQt5",
        "PyQt6",
        "PySide6",
        "numba",
        "matplotlib",
        "openpyxl",
        "PIL",
        "pyarrow",
        "pygame",
        "pytest",
        "sqlalchemy",
        "scipy",
        "tkinter",
        "zmq"
    )
    $PyInstallerArgs = @(
        "--onefile",
        "--name", "kbo_optimizer",
        "--distpath", (Join-Path $RepoRoot "tools"),
        "--workpath", $PyInstallerWork,
        "--specpath", $PyInstallerWork
    )
    foreach ($Module in $PyInstallerExcludes) {
        $PyInstallerArgs += @("--exclude-module", $Module)
    }
    $OrToolsLibDir = & python -c "import ortools, pathlib; print(pathlib.Path(ortools.__file__).parent / '.libs')"
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($OrToolsLibDir)) {
        throw "Could not locate OR-Tools native library directory"
    }
    foreach ($DllName in @("abseil_dll.dll", "libprotobuf.dll", "ortools.dll")) {
        $DllPath = Join-Path $OrToolsLibDir $DllName
        if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
            throw "Required OR-Tools native DLL missing: $DllPath"
        }
        $PyInstallerArgs += @("--add-binary", "$DllPath;.")
    }
    $PyInstallerArgs += $OptimizerSource
    & pyinstaller @PyInstallerArgs
    if ($LASTEXITCODE -ne 0) { throw "Optimizer tool build failed" }
} else {
    Write-Host "==> Optimizer tool is up to date; skipping PyInstaller. Use -RebuildOptimizer to force."
}
if (-not (Test-Path -LiteralPath $OptimizerExe -PathType Leaf)) {
    throw "Optimizer tool build did not produce expected exe: $OptimizerExe"
}

Write-Host "==> Publishing launcher..."
& dotnet publish (Join-Path $RepoRoot "src\KBOLauncher\KBOLauncher.csproj") `
    -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o $Dist
if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed" }

Get-ChildItem $Dist -File | Where-Object { $_.Extension -in ".pdb", ".xml" } | Remove-Item -Force

Write-Host "==> Copying native files..."
Copy-Item (Join-Path $RepoRoot "native\bin\KBOFix.dll") $Dist -Force
Copy-Item (Join-Path $RepoRoot "native\bin\WebView2Loader.dll") $Dist -Force
Copy-Item (Join-Path $RepoRoot "native\kbo_league_id.txt") $Dist -Force

Write-Host "==> Copying UI assets..."
$DistAssets = Join-Path $Dist "assets"
if (Test-Path -LiteralPath $DistAssets) {
    Remove-Item -LiteralPath $DistAssets -Recurse -Force
}
New-Item -ItemType Directory -Path $DistAssets | Out-Null
Copy-Item (Join-Path $RepoRoot "assets\*") $DistAssets -Recurse -Force

Write-Host "==> Copying tool payloads..."
$DistTools = Join-Path $Dist "tools"
if (Test-Path -LiteralPath $DistTools) {
    Remove-Item -LiteralPath $DistTools -Recurse -Force
}
New-Item -ItemType Directory -Path $DistTools | Out-Null
Copy-Item $OptimizerExe $DistTools -Force
Copy-Item $OptimizerSource $DistTools -Force
if (Test-Path -LiteralPath $OptimizerLibSource -PathType Container) {
    Copy-Item $OptimizerLibSource $DistTools -Recurse -Force
}

Write-Host "==> Validating release payload..."
$RequiredFiles = @(
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
$RequiredFiles += Get-SeedManifestPayloadFiles -RepoRoot $RepoRoot
foreach ($RequiredFile in $RequiredFiles) {
    $Path = Join-Path $Dist $RequiredFile
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Release payload missing required file: $RequiredFile"
    }
}
& pwsh -ExecutionPolicy Bypass -File (Join-Path $RepoRoot "tests\release\verify-release-artifact.ps1") -RepoRoot $RepoRoot -Dist $Dist
if ($LASTEXITCODE -ne 0) { throw "Release payload validation failed" }

$FileCount = (Get-ChildItem $Dist -Recurse -File).Count
Write-Host "==> Done: $FileCount files in dist\"
