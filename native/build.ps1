#Requires -Version 7.0
param(
    [switch] $Clean
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$OutDir = Join-Path $Root "bin"
$ObjDir = Join-Path $Root "obj"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

function Resolve-CommandPath {
    param(
        [string[]] $Candidates,

        [Parameter(Mandatory = $true)]
        [string] $Name
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

    throw "Could not find $Name. Install MinGW-w64 GCC, add gcc.exe to PATH, or set KBO_GCC to the full gcc.exe path."
}

function Resolve-WebView2Root {
    $Candidates = @()

    if (-not [string]::IsNullOrWhiteSpace($env:WEBVIEW2_NUGET_ROOT)) {
        $Candidates += $env:WEBVIEW2_NUGET_ROOT
    }

    $NuGetPackagesRoot = if (-not [string]::IsNullOrWhiteSpace($env:NUGET_PACKAGES)) {
        $env:NUGET_PACKAGES
    }
    else {
        Join-Path $env:USERPROFILE ".nuget\packages"
    }

    $PackageRoot = Join-Path $NuGetPackagesRoot "microsoft.web.webview2"
    if (Test-Path -LiteralPath $PackageRoot) {
        $Candidates += Get-ChildItem -LiteralPath $PackageRoot -Directory |
            Sort-Object {
                try {
                    [version]$_.Name
                }
                catch {
                    [version]"0.0"
                }
            } -Descending |
            ForEach-Object { $_.FullName }
    }

    foreach ($Candidate in $Candidates) {
        $Include = Join-Path $Candidate "build\native\include"
        $Loader = Join-Path $Candidate "runtimes\win-x64\native\WebView2Loader.dll"
        if ((Test-Path -LiteralPath $Include) -and (Test-Path -LiteralPath $Loader)) {
            return $Candidate
        }
    }

    throw "Could not find Microsoft.Web.WebView2 native files. Run 'dotnet add package Microsoft.Web.WebView2' or set WEBVIEW2_NUGET_ROOT to the package version directory."
}

function Get-MingwGccCandidates {
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

    return $Candidates
}

function Convert-ToObjectStem {
    param(
        [Parameter(Mandatory = $true)]
        [string] $SourcePath
    )

    $Relative = Get-PathRelativeToRoot -Path $SourcePath
    return ($Relative -replace '[:\\/]+', '_' -replace '\.c$', '')
}

function Get-PathRelativeToRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    $ResolvedRoot = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\') + '\'
    $ResolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $RootUri = [Uri]$ResolvedRoot
    $PathUri = [Uri]$ResolvedPath
    return [Uri]::UnescapeDataString($RootUri.MakeRelativeUri($PathUri).ToString()).Replace('/', '\')
}

function Read-DependencyPaths {
    param(
        [Parameter(Mandatory = $true)]
        [string] $DepPath
    )

    if (-not (Test-Path -LiteralPath $DepPath)) {
        return @()
    }

    $Text = Get-Content -LiteralPath $DepPath -Raw
    $Text = $Text -replace "\\\r?\n", " "
    $ColonMatch = [regex]::Match($Text, ":\s")
    if (-not $ColonMatch.Success) {
        return @()
    }

    $DependencyText = $Text.Substring($ColonMatch.Index + $ColonMatch.Length)
    return $DependencyText -split "\s+" |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        ForEach-Object { $_ -replace "\\ ", " " } |
        Where-Object { Test-Path -LiteralPath $_ }
}

function Test-ObjectOutOfDate {
    param(
        [Parameter(Mandatory = $true)]
        [string] $SourcePath,

        [Parameter(Mandatory = $true)]
        [string] $ObjectPath,

        [Parameter(Mandatory = $true)]
        [string] $DepPath
    )

    if (-not (Test-Path -LiteralPath $ObjectPath)) {
        return $true
    }

    if (-not (Test-Path -LiteralPath $DepPath)) {
        return $true
    }

    $ObjectTime = (Get-Item -LiteralPath $ObjectPath).LastWriteTimeUtc
    if ((Get-Item -LiteralPath $SourcePath).LastWriteTimeUtc -gt $ObjectTime) {
        return $true
    }

    foreach ($Dependency in Read-DependencyPaths -DepPath $DepPath) {
        if (-not (Test-Path -LiteralPath $Dependency)) {
            return $true
        }
        if ((Get-Item -LiteralPath $Dependency).LastWriteTimeUtc -gt $ObjectTime) {
            return $true
        }
    }

    return $false
}

function Test-LinkOutOfDate {
    param(
        [Parameter(Mandatory = $true)]
        [string] $OutputPath,

        [Parameter(Mandatory = $true)]
        [string[]] $ObjectPaths,

        [Parameter(Mandatory = $true)]
        [string] $LoaderPath
    )

    if (-not (Test-Path -LiteralPath $OutputPath)) {
        return $true
    }

    $OutputTime = (Get-Item -LiteralPath $OutputPath).LastWriteTimeUtc
    if ((Get-Item -LiteralPath $LoaderPath).LastWriteTimeUtc -gt $OutputTime) {
        return $true
    }

    foreach ($ObjectPath in $ObjectPaths) {
        if ((Get-Item -LiteralPath $ObjectPath).LastWriteTimeUtc -gt $OutputTime) {
            return $true
        }
    }

    return $false
}

function Convert-ToGccResponsePath {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    return (Resolve-Path -LiteralPath $Path).Path.Replace('\', '/')
}

function Copy-ItemIfNewer {
    param(
        [Parameter(Mandatory = $true)]
        [string] $SourcePath,

        [Parameter(Mandatory = $true)]
        [string] $DestinationPath
    )

    if (-not (Test-Path -LiteralPath $SourcePath)) {
        return $false
    }

    if (Test-Path -LiteralPath $DestinationPath) {
        $SourceTime = (Get-Item -LiteralPath $SourcePath).LastWriteTimeUtc
        $DestinationTime = (Get-Item -LiteralPath $DestinationPath).LastWriteTimeUtc
        if ($DestinationTime -ge $SourceTime) {
            return $false
        }
    }

    Copy-Item -LiteralPath $SourcePath -Destination $DestinationPath -Force
    return $true
}

function Copy-DirectoryFilesIfNewer {
    param(
        [Parameter(Mandatory = $true)]
        [string] $SourceDir,

        [Parameter(Mandatory = $true)]
        [string] $DestinationDir
    )

    if (-not (Test-Path -LiteralPath $SourceDir)) {
        return 0
    }

    $CopiedCount = 0
    Get-ChildItem -LiteralPath $SourceDir -Recurse -File | ForEach-Object {
        $Relative = Get-PathRelativeToRoot -Path $_.FullName
        $RelativeToSource = $Relative.Substring((Get-PathRelativeToRoot -Path $SourceDir).Length).TrimStart('\')
        $DestinationPath = Join-Path $DestinationDir $RelativeToSource
        $DestinationParent = Split-Path -Parent $DestinationPath
        New-Item -ItemType Directory -Force -Path $DestinationParent | Out-Null
        if (Copy-ItemIfNewer -SourcePath $_.FullName -DestinationPath $DestinationPath) {
            $CopiedCount += 1
        }
    }
    return $CopiedCount
}

function Get-LatestWriteTimeUtc {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.FileInfo[]] $Files
    )

    $Latest = [datetime]::MinValue
    foreach ($File in $Files) {
        if ($File.LastWriteTimeUtc -gt $Latest) {
            $Latest = $File.LastWriteTimeUtc
        }
    }
    return $Latest
}

$Gcc = Resolve-CommandPath -Name "gcc.exe" -Candidates @(
    Get-MingwGccCandidates
)

$WebView2Root = Resolve-WebView2Root
$WebView2Include = Join-Path $WebView2Root "build\native\include"
$WebView2Loader = Join-Path $WebView2Root "runtimes\win-x64\native\WebView2Loader.dll"

if ($Clean) {
    Remove-Item -LiteralPath $ObjDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $OutDir "KBOFix.dll") -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null
}

Copy-ItemIfNewer -SourcePath $WebView2Loader -DestinationPath (Join-Path $OutDir "WebView2Loader.dll") | Out-Null

Write-Host "GCC: $Gcc"
Write-Host "WebView2: $WebView2Root"

$NativeSources = @(
    Join-Path $Root "KBOFix.c"
)
$NativeSources += Get-ChildItem -LiteralPath (Join-Path $Root "src") -Recurse -Filter "*.c" |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }
$NativeSources += Get-ChildItem -LiteralPath (Join-Path $Root "third_party\csv_fast_reader") -Filter "*.c" |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }

$DllPath = Join-Path $OutDir "KBOFix.dll"
$NativeInputFiles = @()
$NativeInputFiles += Get-Item -LiteralPath $NativeSources
$NativeInputFiles += Get-ChildItem -LiteralPath (Join-Path $Root "src") -Recurse -Include "*.h" -File
$NativeInputFiles += Get-ChildItem -LiteralPath (Join-Path $Root "third_party") -Recurse -Include "*.h", "*.c" -File
$NativeInputFiles += Get-Item -LiteralPath $WebView2Loader

$InputsNewestThanDll = $true
if ((Test-Path -LiteralPath $DllPath) -and -not $Clean) {
    $InputsNewestThanDll = (Get-LatestWriteTimeUtc -Files $NativeInputFiles) -gt (Get-Item -LiteralPath $DllPath).LastWriteTimeUtc
}

$CompiledCount = 0
$SkipNativeBuild = (Test-Path -LiteralPath $DllPath) -and -not $InputsNewestThanDll -and -not $Clean
if ($SkipNativeBuild) {
    Write-Host "KBOFix.dll is up to date; skipped native dependency scan ($($NativeSources.Count) sources)."
    $Objects = $NativeSources | ForEach-Object {
        $Stem = Convert-ToObjectStem -SourcePath $_
        Join-Path $ObjDir "$Stem.o"
    }
}
else {
    $SourceEntries = $NativeSources | ForEach-Object {
        $Stem = Convert-ToObjectStem -SourcePath $_
        [PSCustomObject]@{
            Source     = $_
            ObjectPath = Join-Path $ObjDir "$Stem.o"
            DepPath    = Join-Path $ObjDir "$Stem.d"
        }
    }

    $Objects = $SourceEntries | ForEach-Object { $_.ObjectPath }

    $ToCompile = $SourceEntries | Where-Object {
        Test-ObjectOutOfDate -SourcePath $_.Source -ObjectPath $_.ObjectPath -DepPath $_.DepPath
    }

    if ($ToCompile) {
        $GccPath             = $Gcc
        $WebView2IncludePath = $WebView2Include
        $SourceIncludePath   = Join-Path $Root "src"
        $ThrottleLimit       = [Environment]::ProcessorCount
        Write-Host "Compiling $($ToCompile.Count) of $($NativeSources.Count) sources (parallel, $ThrottleLimit threads)"

        $Results = $ToCompile | ForEach-Object -Parallel {
            $Entry      = $_
            $GccPath    = $using:GccPath
            $IncludePath = $using:WebView2IncludePath
            $SourceIncludePath = $using:SourceIncludePath

            $Output = & $GccPath -O2 -pipe -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
                -isystem $IncludePath `
                -I $SourceIncludePath `
                -MMD -MP -MF $Entry.DepPath `
                -c $Entry.Source `
                -o $Entry.ObjectPath 2>&1

            [PSCustomObject]@{
                Source   = $Entry.Source
                ExitCode = $LASTEXITCODE
                Output   = $Output
            }
        } -ThrottleLimit $ThrottleLimit

        foreach ($Result in $Results) {
            if ($Result.Output) { $Result.Output | ForEach-Object { Write-Host $_ } }
            if ($Result.ExitCode -ne 0) {
                throw "Failed to compile $($Result.Source)"
            }
            $CompiledCount += 1
        }
    }

    if (Test-LinkOutOfDate -OutputPath $DllPath -ObjectPaths $Objects -LoaderPath $WebView2Loader) {
        Write-Host "Linking KBOFix.dll ($($Objects.Count) objects, $CompiledCount compiled)"
        $ResponsePath = Join-Path $ObjDir "KBOFix.link.rsp"
        $ResponseArgs = @(
            "-shared"
            "-static"
            "-static-libgcc"
            "-o"
            "`"$($DllPath.Replace('\', '/'))`""
        )
        $ResponseArgs += $Objects | ForEach-Object { "`"$(Convert-ToGccResponsePath -Path $_)`"" }
        $ResponseArgs += @(
            "-lgdi32"
            "-lmsimg32"
            "-lole32"
            "-luuid"
            "-lshell32"
            "-lwindowscodecs"
        )
        Set-Content -LiteralPath $ResponsePath -Value ($ResponseArgs -join " ") -Encoding ASCII
        $PreviousErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $GccOutput = & $Gcc -shared `
            "@$ResponsePath" 2>&1
        $GccExitCode = $LASTEXITCODE
        $ErrorActionPreference = $PreviousErrorActionPreference
        $GccOutput | ForEach-Object { Write-Host $_ }
        if ($GccExitCode -ne 0) {
            throw "Failed to link KBOFix.dll"
        }
    }
    else {
        Write-Host "KBOFix.dll is up to date ($($Objects.Count) objects, $CompiledCount compiled)"
    }
}

if ($CompiledCount -eq 0) {
    Write-Host "No native sources changed."
}

Write-Host "Built $DllPath"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$LauncherOutputDirs = @(
    (Join-Path $RepoRoot "bin\Release\net8.0"),
    (Join-Path $RepoRoot "bin\Debug\net8.0"),
    (Join-Path $RepoRoot "bin\Release\net8.0\win-x64"),
    (Join-Path $RepoRoot "bin\Debug\net8.0\win-x64"),
    (Join-Path $RepoRoot "src\KBOLauncher\bin\Release\net8.0"),
    (Join-Path $RepoRoot "src\KBOLauncher\bin\Debug\net8.0"),
    (Join-Path $RepoRoot "src\KBOLauncher\bin\Release\net8.0\win-x64"),
    (Join-Path $RepoRoot "src\KBOLauncher\bin\Debug\net8.0\win-x64")
)
foreach ($LauncherOutputDir in $LauncherOutputDirs) {
    if (Test-Path $LauncherOutputDir) {
        $SyncedCount = 0
        if (Copy-ItemIfNewer -SourcePath $DllPath -DestinationPath (Join-Path $LauncherOutputDir "KBOFix.dll")) {
            $SyncedCount += 1
        }
        $LoaderPath = Join-Path $OutDir "WebView2Loader.dll"
        if (Copy-ItemIfNewer -SourcePath $LoaderPath -DestinationPath (Join-Path $LauncherOutputDir "WebView2Loader.dll")) {
            $SyncedCount += 1
        }
        $AssetsPath = Join-Path $OutDir "assets"
        $SyncedCount += Copy-DirectoryFilesIfNewer -SourceDir $AssetsPath -DestinationDir (Join-Path $LauncherOutputDir "assets")
        $SeedPath = Join-Path $RepoRoot "data\seeds"
        if (Test-Path $SeedPath) {
            $SyncedCount += Copy-DirectoryFilesIfNewer -SourceDir $SeedPath -DestinationDir (Join-Path $LauncherOutputDir "data\seeds")
        }
        Write-Host "Synced native payload to $LauncherOutputDir ($SyncedCount files copied)"
    }
}
