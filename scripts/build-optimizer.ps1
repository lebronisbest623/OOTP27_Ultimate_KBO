param(
    [switch]$RebuildOptimizer,
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
)

$ErrorActionPreference = "Stop"

$OptimizerSource = Join-Path $RepoRoot "tools\kbo_optimizer.py"
$OptimizerLibSource = Join-Path $RepoRoot "tools\kbo_optimizer_lib"
$PyInstallerWork = Join-Path $RepoRoot "obj\pyinstaller"
$OptimizerExe = Join-Path $RepoRoot "tools\kbo_optimizer.exe"

$OptimizerNewestSourceTime = (Get-Item -LiteralPath $OptimizerSource).LastWriteTimeUtc
Get-ChildItem -LiteralPath $OptimizerLibSource -Recurse -Filter *.py | ForEach-Object {
    if ($_.LastWriteTimeUtc -gt $OptimizerNewestSourceTime) {
        $OptimizerNewestSourceTime = $_.LastWriteTimeUtc
    }
}

$OptimizerNeedsBuild = $RebuildOptimizer `
    -or -not (Test-Path -LiteralPath $OptimizerExe -PathType Leaf) `
    -or ((Get-Item -LiteralPath $OptimizerExe).LastWriteTimeUtc -lt $OptimizerNewestSourceTime)

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
