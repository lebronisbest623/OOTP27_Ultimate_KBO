param(
    [string]$RepoRoot = "",
    [string]$Python = "python",
    [string]$Optimizer = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}

if ([string]::IsNullOrWhiteSpace($Optimizer)) {
    $Optimizer = Join-Path $RepoRoot "tools\kbo_optimizer.py"
}

if (-not (Test-Path -LiteralPath $Optimizer -PathType Leaf)) {
    throw "Optimizer script not found: $Optimizer"
}

$FixtureDir = Join-Path $PSScriptRoot "fixtures"
$GoldenDir = Join-Path $PSScriptRoot "golden"
$WorkDir = Join-Path ([System.IO.Path]::GetTempPath()) ("kbo-optimizer-golden-" + [guid]::NewGuid().ToString("N"))

function Assert-FileBytesEqual {
    param(
        [Parameter(Mandatory = $true)][string]$ExpectedPath,
        [Parameter(Mandatory = $true)][string]$ActualPath,
        [Parameter(Mandatory = $true)][string]$Label
    )

    [byte[]]$expected = [System.IO.File]::ReadAllBytes($ExpectedPath)
    [byte[]]$actual = [System.IO.File]::ReadAllBytes($ActualPath)
    if ($expected.Length -ne $actual.Length) {
        throw "$Label byte length mismatch: expected $($expected.Length), actual $($actual.Length)"
    }

    for ($i = 0; $i -lt $expected.Length; $i++) {
        if ($expected[$i] -ne $actual[$i]) {
            throw "$Label byte mismatch at offset $i`: expected $($expected[$i]), actual $($actual[$i])"
        }
    }
}

function Invoke-OptimizerMode {
    param(
        [Parameter(Mandatory = $true)][string]$Mode,
        [Parameter(Mandatory = $true)][string]$RequestPath,
        [Parameter(Mandatory = $true)][string]$ResultPath
    )

    & $Python $Optimizer --mode $Mode $RequestPath $ResultPath
    if ($LASTEXITCODE -ne 0) {
        throw "Optimizer mode '$Mode' failed with exit code $LASTEXITCODE"
    }
}

try {
    New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null

    $cases = @(
        @{ Mode = "amateur_assignment"; Fixture = "amateur_assignment.csv"; Golden = "amateur_assignment.csv"; CheckDefaultCli = $true },
        @{ Mode = "amateur_assignment"; Fixture = "amateur_assignment_batch.csv"; Golden = "amateur_assignment_batch.csv"; CheckDefaultCli = $true },
        @{ Mode = "asian_games_roster"; Fixture = "asian_games_roster.csv"; Golden = "asian_games_roster.csv"; CheckDefaultCli = $false },
        @{ Mode = "military_selection"; Fixture = "military_selection.csv"; Golden = "military_selection.csv"; CheckDefaultCli = $false },
        @{ Mode = "fa_compensation"; Fixture = "fa_compensation.csv"; Golden = "fa_compensation.csv"; CheckDefaultCli = $false }
    )

    foreach ($case in $cases) {
        $request = Join-Path $FixtureDir $case.Fixture
        $golden = Join-Path $GoldenDir $case.Golden
        $actual = Join-Path $WorkDir ($case.Mode + ".csv")
        Invoke-OptimizerMode -Mode $case.Mode -RequestPath $request -ResultPath $actual
        Assert-FileBytesEqual -ExpectedPath $golden -ActualPath $actual -Label $case.Mode

        if ($case.CheckDefaultCli) {
            $defaultActual = Join-Path $WorkDir ($case.Mode + ".default.csv")
            & $Python $Optimizer $request $defaultActual
            if ($LASTEXITCODE -ne 0) {
                throw "Optimizer default CLI failed with exit code $LASTEXITCODE"
            }
            Assert-FileBytesEqual -ExpectedPath $golden -ActualPath $defaultActual -Label "$($case.Mode) default CLI"
        }
    }

    $serverAmateur = Join-Path $WorkDir "server_amateur_assignment.csv"
    $serverMilitary = Join-Path $WorkDir "server_military_selection.csv"
    $serverInput = @(
        "amateur_assignment`t$(Join-Path $FixtureDir 'amateur_assignment.csv')`t$serverAmateur",
        "military_selection`t$(Join-Path $FixtureDir 'military_selection.csv')`t$serverMilitary",
        "bad_request"
    ) -join [Environment]::NewLine

    $serverOutput = $serverInput | & $Python $Optimizer --server
    if ($LASTEXITCODE -ne 0) {
        throw "Optimizer server failed with exit code $LASTEXITCODE"
    }

    $expectedServerOutput = @("OK 0", "OK 0", "ERR bad_request")
    if (@($serverOutput).Count -ne $expectedServerOutput.Count) {
        throw "Server output line count mismatch: expected $($expectedServerOutput.Count), actual $(@($serverOutput).Count)"
    }
    for ($i = 0; $i -lt $expectedServerOutput.Count; $i++) {
        if ($serverOutput[$i] -ne $expectedServerOutput[$i]) {
            throw "Server output mismatch at line $($i + 1): expected '$($expectedServerOutput[$i])', actual '$($serverOutput[$i])'"
        }
    }

    Assert-FileBytesEqual `
        -ExpectedPath (Join-Path $GoldenDir "amateur_assignment.csv") `
        -ActualPath $serverAmateur `
        -Label "server amateur_assignment"
    Assert-FileBytesEqual `
        -ExpectedPath (Join-Path $GoldenDir "military_selection.csv") `
        -ActualPath $serverMilitary `
        -Label "server military_selection"

    Write-Host "KBO optimizer golden fixtures verified."
}
finally {
    if (Test-Path -LiteralPath $WorkDir) {
        Remove-Item -LiteralPath $WorkDir -Recurse -Force
    }
}
