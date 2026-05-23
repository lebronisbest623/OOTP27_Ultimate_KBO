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
        @{ Mode = "asian_games_roster"; Fixture = "asian_games_roster_wildcards.csv"; Golden = "asian_games_roster_wildcards.csv"; CheckDefaultCli = $false },
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

    $attachedRequest = Join-Path $FixtureDir "amateur_assignment_attached_batch.csv"
    $attachedActual = Join-Path $WorkDir "amateur_assignment_attached_batch.csv"
    Invoke-OptimizerMode -Mode "amateur_assignment" -RequestPath $attachedRequest -ResultPath $attachedActual
    $attachedRows = Import-Csv -LiteralPath $attachedActual
    $protectedCount = @($attachedRows | Where-Object { $_.target_team_id -eq "10" }).Count
    $openCount = @($attachedRows | Where-Object { $_.target_team_id -eq "20" }).Count
    if ($protectedCount -ne 20 -or $openCount -ne 0) {
        throw "incoming_attached feeder minimum regression: expected team 10=20 and team 20=0, actual team 10=$protectedCount team 20=$openCount"
    }

    $floorRequest = Join-Path $WorkDir "amateur_assignment_high_school_floor.csv"
    $floorActual = Join-Path $WorkDir "amateur_assignment_high_school_floor_result.csv"
    $floorLines = [System.Collections.Generic.List[string]]::new()
    $floorLines.Add("player_id,current_team_id,player_tier,team_id,team_tier,reputation,target_reputation,player_count,hitter_count,league_id,target_max_players,rejected,quality_score,is_hitter,role_bucket,batch_mode,pitcher_count,catcher_count,first_base_count,second_base_count,third_base_count,shortstop_count,left_field_count,center_field_count,right_field_count,designated_hitter_count")
    for ($i = 1; $i -le 8; $i++) {
        $playerId = 7000 + $i
        $floorLines.Add("$playerId,0,3,10,3,50,50,18,18,203,34,0,1000,0,P,incoming,0,2,2,2,2,2,3,3,2,0")
        $floorLines.Add("$playerId,0,3,20,3,50,50,18,0,203,34,0,1000,0,P,incoming,18,0,0,0,0,0,0,0,0,0")
    }
    for ($i = 1; $i -le 11; $i++) {
        $playerId = 7100 + $i
        $floorLines.Add("$playerId,0,3,10,3,50,50,18,18,203,34,0,1000,1,C,incoming,0,2,2,2,2,2,3,3,2,0")
        $floorLines.Add("$playerId,0,3,20,3,50,50,18,0,203,34,0,1000,1,C,incoming,18,0,0,0,0,0,0,0,0,0")
    }
    [System.IO.File]::WriteAllLines($floorRequest, $floorLines, [System.Text.UTF8Encoding]::new($false))
    Invoke-OptimizerMode -Mode "amateur_assignment" -RequestPath $floorRequest -ResultPath $floorActual
    $floorRows = Import-Csv -LiteralPath $floorActual
    $floorTeam10Pitchers = @($floorRows | Where-Object { $_.target_team_id -eq "10" -and [int]$_.player_id -lt 7100 }).Count
    $floorTeam20Hitters = @($floorRows | Where-Object { $_.target_team_id -eq "20" -and [int]$_.player_id -ge 7100 }).Count
    if ($floorTeam10Pitchers -ne 8 -or $floorTeam20Hitters -ne 11) {
        throw "high-school floor regression: expected team 10 to receive 8 pitchers and team 20 to receive 11 hitters, actual team 10 pitchers=$floorTeam10Pitchers team 20 hitters=$floorTeam20Hitters"
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
