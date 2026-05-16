$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$TestSrc = Join-Path $PSScriptRoot "test_main.c"
$TestExe = Join-Path $PSScriptRoot "tests.exe"
$WebViewCommandRouterTestSrc = Join-Path $PSScriptRoot "test_webview_command_router.c"
$WebViewCommandRouterTestExe = Join-Path $PSScriptRoot "test_webview_command_router.exe"

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

$Gcc = Resolve-Gcc
Write-Host "GCC: $Gcc"

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $WebViewCommandRouterTestExe `
    $WebViewCommandRouterTestSrc
if ($LASTEXITCODE -ne 0) {
    throw "WebView command router test build failed"
}

& $WebViewCommandRouterTestExe
if ($LASTEXITCODE -ne 0) {
    throw "WebView command router tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $TestExe `
    $TestSrc `
    (Join-Path $Root "src\allstar\csv\allstar_csv_parse.c") `
    (Join-Path $Root "src\allstar\allstar_native_events\schedule\schedule_dates.c") `
    (Join-Path $Root "src\allstar\allstar_native_events\schedule\file\schedule_dates_file.c") `
    (Join-Path $Root "src\core\csv\core_csv.c") `
    (Join-Path $Root "src\foreign\common\dates\foreign_waiver_date.c") `
    (Join-Path $Root "src\foreign\replacement_seed\parse\foreign_replacement_seed_parse.c") `
    (Join-Path $Root "src\captain\season\captain_season.c") `
    (Join-Path $Root "src\core\season\phase\season_phase_rules.c") `
    (Join-Path $Root "src\captain\seed\parse\captain_seed_parse.c") `
    (Join-Path $Root "src\military_service\calendar\military_service_date.c") `
    (Join-Path $Root "src\military_service\selection\events\policy\military_selection_policy.c") `
    (Join-Path $Root "src\military_service\seed\parse\military_service_seed_parse.c") `
    (Join-Path $Root "src\military_service\players\team_policy\military_service_team_policy_parse.c") `
    (Join-Path $Root "src\team\classification\parse\team_classification_seed_parse.c") `
    (Join-Path $Root "src\fa_market_classification\policy\fa_market_row_policy.c") `
    (Join-Path $Root "src\team\names\team_string.c") `
    (Join-Path $Root "src\core\dates\core_text_date.c") `
    (Join-Path $Root "src\core\sql\escape\core_sql_escape.c") `
    (Join-Path $Root "src\core\core_flags\keys\flag_key.c") `
    (Join-Path $Root "src\core\core_flags\json\json_bool_parser.c") `
    (Join-Path $Root "src\core\core_flags\json\json_string_decode.c") `
    (Join-Path $Root "src\awards\schedule\policy\award_schedule_policy_parse.c") `
    (Join-Path $Root "src\core\news\templates\render\core_news_template_render.c") `
    (Join-Path $Root "src\core\news\links\core_news_links.c") `
    (Join-Path $Root "src\core\core_flags\api\settings\custom_news_language.c") `
    (Join-Path $Root "src\core\core_flags\localappdata\localappdata_reader.c") `
    (Join-Path $Root "src\core\core_flags\api\settings\economic\economic_defaults.c") `
    (Join-Path $Root "src\core\core_flags\api\settings\foreign\foreign_demand_baselines.c") `
    (Join-Path $Root "src\amateur_player_quality\assignment\policy\amateur_assignment_policy_values.c") `
    (Join-Path $Root "src\patch_helpers\bytes\patch_bytes.c") `
    (Join-Path $Root "src\fa_filing\fa_filing_parts\fa_filing_csv_parse.c") `
    (Join-Path $Root "src\fa_salary_snapshot\csv\salary_snapshot_csv_parse.c") `
    (Join-Path $Root "src\core\files\atomic\core_atomic_file.c") `
    (Join-Path $Root "src\core\files\save_paths\platform\windows_path.c") `
    (Join-Path $Root "src\core\logging\event\log_event.c") `
    (Join-Path $Root "src\core\logging\rule_audit.c") `
    (Join-Path $Root "src\core\policy\core_policy.c") `
    (Join-Path $Root "src\military_service\players\loans\military_native_loan.c") `
    (Join-Path $Root "src\team\assignment\roster_arrays\team_roster_arrays.c") `
    (Join-Path $Root "src\foreign\common\policy\foreign_player_policy.c") `
    (Join-Path $Root "src\foreign\common\player_eval\foreign_waiver_player_eval.c") `
    (Join-Path $Root "src\foreign\injury\state\foreign_injury_state.c") `
    (Join-Path $Root "src\team\independent_acquisition\ai\independent_acquisition_score.c") `
    (Join-Path $Root "src\amateur_player_quality\assignment\policy\amateur_assignment_policy.c")
if ($LASTEXITCODE -ne 0) {
    throw "Build failed"
}

& $TestExe
if ($LASTEXITCODE -ne 0) {
    throw "Tests failed"
}

Write-Host "All native tests passed."
