$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$TestSrc = Join-Path $PSScriptRoot "test_main.c"
$TestExe = Join-Path $PSScriptRoot "tests.exe"
$FaCompensationSelectionTestSrc = Join-Path $PSScriptRoot "test_fa_compensation_selection.c"
$FaCompensationSelectionTestExe = Join-Path $PSScriptRoot "test_fa_compensation_selection.exe"
$ForeignRetentionCandidateGateTestSrc = Join-Path $PSScriptRoot "test_foreign_retention_candidate_gate.c"
$ForeignRetentionCandidateGateTestExe = Join-Path $PSScriptRoot "test_foreign_retention_candidate_gate.exe"
$ForeignRetentionScoreGateTestSrc = Join-Path $PSScriptRoot "test_foreign_retention_score_gate.c"
$ForeignRetentionScoreGateTestExe = Join-Path $PSScriptRoot "test_foreign_retention_score_gate.exe"
$ForeignOfferAttachHookPolicyTestSrc = Join-Path $PSScriptRoot "test_foreign_offer_attach_hook_policy.c"
$ForeignOfferAttachHookPolicyTestExe = Join-Path $PSScriptRoot "test_foreign_offer_attach_hook_policy.exe"
$ForeignAiOfferContractTypeTestSrc = Join-Path $PSScriptRoot "test_foreign_ai_offer_contract_type.c"
$ForeignAiOfferContractTypeTestExe = Join-Path $PSScriptRoot "test_foreign_ai_offer_contract_type.exe"
$ForeignFaFinancialsWriteTestSrc = Join-Path $PSScriptRoot "test_foreign_fa_financials_write.c"
$ForeignFaFinancialsWriteTestExe = Join-Path $PSScriptRoot "test_foreign_fa_financials_write.exe"
$OfferCandidateReplacementDispatcherTestSrc = Join-Path $PSScriptRoot "test_offer_candidate_replacement_dispatcher.c"
$OfferCandidateReplacementDispatcherTestExe = Join-Path $PSScriptRoot "test_offer_candidate_replacement_dispatcher.exe"
$DomesticFaOrphanRescuePolicyTestSrc = Join-Path $PSScriptRoot "test_domestic_fa_orphan_rescue_policy.c"
$DomesticFaOrphanRescuePolicyTestExe = Join-Path $PSScriptRoot "test_domestic_fa_orphan_rescue_policy.exe"
$DomesticFaOrphanRescueOfferReplacementTestSrc = Join-Path $PSScriptRoot "test_domestic_fa_orphan_rescue_offer_replacement.c"
$DomesticFaOrphanRescueOfferReplacementTestExe = Join-Path $PSScriptRoot "test_domestic_fa_orphan_rescue_offer_replacement.exe"
$IndependentAcquisitionCashFlowTestSrc = Join-Path $PSScriptRoot "test_independent_acquisition_cash_flow.c"
$IndependentAcquisitionCashFlowTestExe = Join-Path $PSScriptRoot "test_independent_acquisition_cash_flow.exe"
$IntlEstablishedFaMarketNormalizeTestSrc = Join-Path $PSScriptRoot "test_intl_established_fa_market_normalize.c"
$IntlEstablishedFaMarketNormalizeTestExe = Join-Path $PSScriptRoot "test_intl_established_fa_market_normalize.exe"
$NoMinorDemandClassifyTestSrc = Join-Path $PSScriptRoot "test_no_minor_demand_classify.c"
$NoMinorDemandClassifyTestExe = Join-Path $PSScriptRoot "test_no_minor_demand_classify.exe"
$ForeignNoMinorContractRepairTestSrc = Join-Path $PSScriptRoot "test_foreign_no_minor_contract_repair.c"
$ForeignNoMinorContractRepairTestExe = Join-Path $PSScriptRoot "test_foreign_no_minor_contract_repair.exe"
$ForeignInjuryExistingReplacementsTestSrc = Join-Path $PSScriptRoot "test_foreign_injury_existing_replacements.c"
$ForeignInjuryExistingReplacementsTestExe = Join-Path $PSScriptRoot "test_foreign_injury_existing_replacements.exe"
$IntlEstablishedFaObservedPlayersTestSrc = Join-Path $PSScriptRoot "test_intl_established_fa_observed_players.c"
$IntlEstablishedFaObservedPlayersTestExe = Join-Path $PSScriptRoot "test_intl_established_fa_observed_players.exe"
$TeamLookupPlayerVectorCacheTestSrc = Join-Path $PSScriptRoot "test_team_lookup_player_vector_cache.c"
$TeamLookupPlayerVectorCacheTestExe = Join-Path $PSScriptRoot "test_team_lookup_player_vector_cache.exe"
$IntlEstablishedFaEventTimingTestSrc = Join-Path $PSScriptRoot "test_intl_established_fa_event_timing.c"
$IntlEstablishedFaEventTimingTestExe = Join-Path $PSScriptRoot "test_intl_established_fa_event_timing.exe"
$AsianGamesHandlerSaveContextTestSrc = Join-Path $PSScriptRoot "test_asian_games_handler_save_context.c"
$AsianGamesHandlerSaveContextTestExe = Join-Path $PSScriptRoot "test_asian_games_handler_save_context.exe"
$AsianGamesPlayerEligibilityTestSrc = Join-Path $PSScriptRoot "test_asian_games_player_eligibility.c"
$AsianGamesPlayerEligibilityTestExe = Join-Path $PSScriptRoot "test_asian_games_player_eligibility.exe"
$AsianGamesRestrictedMaintenancePolicyTestSrc = Join-Path $PSScriptRoot "test_asian_games_restricted_maintenance_policy.c"
$AsianGamesRestrictedMaintenancePolicyTestExe = Join-Path $PSScriptRoot "test_asian_games_restricted_maintenance_policy.exe"
$FaDeclarationRepairTestSrc = Join-Path $PSScriptRoot "test_fa_declaration_repair.c"
$FaDeclarationRepairTestExe = Join-Path $PSScriptRoot "test_fa_declaration_repair.exe"
$FaDeclarationContractTestSrc = Join-Path $PSScriptRoot "test_fa_declaration_contract.c"
$FaDeclarationContractTestExe = Join-Path $PSScriptRoot "test_fa_declaration_contract.exe"
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
    -o $FaCompensationSelectionTestExe `
    $FaCompensationSelectionTestSrc `
    (Join-Path $Root "src\fa_compensation\selection\fa_compensation_selection.c")
if ($LASTEXITCODE -ne 0) {
    throw "FA compensation selection test build failed"
}

& $FaCompensationSelectionTestExe
if ($LASTEXITCODE -ne 0) {
    throw "FA compensation selection tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $ForeignRetentionCandidateGateTestExe `
    $ForeignRetentionCandidateGateTestSrc `
    (Join-Path $Root "src\foreign\signability\foreign_policy\wrappers\retained_candidates\foreign_signability_retained_candidate_gate.c")
if ($LASTEXITCODE -ne 0) {
    throw "Foreign retention candidate gate test build failed"
}

& $ForeignRetentionCandidateGateTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Foreign retention candidate gate tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $ForeignRetentionScoreGateTestExe `
    $ForeignRetentionScoreGateTestSrc `
    (Join-Path $Root "src\foreign\quota\candidates\retention_score\foreign_quota_retention_score_gate.c")
if ($LASTEXITCODE -ne 0) {
    throw "Foreign retention score gate test build failed"
}

& $ForeignRetentionScoreGateTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Foreign retention score gate tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $ForeignOfferAttachHookPolicyTestExe `
    $ForeignOfferAttachHookPolicyTestSrc `
    (Join-Path $Root "src\foreign\signability\foreign_policy\wrappers\offer_attach\install_policy\foreign_ai_offer_attach_hook_policy.c")
if ($LASTEXITCODE -ne 0) {
    throw "Foreign offer attach hook policy test build failed"
}

& $ForeignOfferAttachHookPolicyTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Foreign offer attach hook policy tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $ForeignAiOfferContractTypeTestExe `
    $ForeignAiOfferContractTypeTestSrc `
    (Join-Path $Root "src\foreign\signability\foreign_policy\wrappers\offer_attach\foreign_ai_offer_contract_type.c")
if ($LASTEXITCODE -ne 0) {
    throw "Foreign AI offer contract type test build failed"
}

& $ForeignAiOfferContractTypeTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Foreign AI offer contract type tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $ForeignFaFinancialsWriteTestExe `
    $ForeignFaFinancialsWriteTestSrc `
    (Join-Path $Root "src\foreign\signability\submit_offer_probe\demand\submit_offer_probe_foreign_fa_demand_restore.c")
if ($LASTEXITCODE -ne 0) {
    throw "Foreign FA financials write test build failed"
}

& $ForeignFaFinancialsWriteTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Foreign FA financials write tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $OfferCandidateReplacementDispatcherTestExe `
    $OfferCandidateReplacementDispatcherTestSrc `
    (Join-Path $Root "src\offer_candidate\replacement\offer_candidate_replacement_dispatcher.c")
if ($LASTEXITCODE -ne 0) {
    throw "Offer candidate replacement dispatcher test build failed"
}

& $OfferCandidateReplacementDispatcherTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Offer candidate replacement dispatcher tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $DomesticFaOrphanRescuePolicyTestExe `
    $DomesticFaOrphanRescuePolicyTestSrc `
    (Join-Path $Root "src\fa_market_investigation\rescue\domestic_fa_orphan_rescue_policy.c")
if ($LASTEXITCODE -ne 0) {
    throw "Domestic FA orphan rescue policy test build failed"
}

& $DomesticFaOrphanRescuePolicyTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Domestic FA orphan rescue policy tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $DomesticFaOrphanRescueOfferReplacementTestExe `
    $DomesticFaOrphanRescueOfferReplacementTestSrc `
    (Join-Path $Root "src\fa_market_investigation\rescue\offer_replacement\domestic_fa_orphan_rescue_offer_replacement.c")
if ($LASTEXITCODE -ne 0) {
    throw "Domestic FA orphan rescue offer replacement test build failed"
}

& $DomesticFaOrphanRescueOfferReplacementTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Domestic FA orphan rescue offer replacement tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $IndependentAcquisitionCashFlowTestExe `
    $IndependentAcquisitionCashFlowTestSrc `
    (Join-Path $Root "src\team\independent_acquisition\ai\buyer\independent_acquisition_buyer_state.c")
if ($LASTEXITCODE -ne 0) {
    throw "Independent acquisition cash flow test build failed"
}

& $IndependentAcquisitionCashFlowTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Independent acquisition cash flow tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $IntlEstablishedFaMarketNormalizeTestExe `
    $IntlEstablishedFaMarketNormalizeTestSrc `
    (Join-Path $Root "src\foreign\intl_established_fa_postscan\market\intl_established_fa_market_normalize.c")
if ($LASTEXITCODE -ne 0) {
    throw "International established FA market normalization test build failed"
}

& $IntlEstablishedFaMarketNormalizeTestExe
if ($LASTEXITCODE -ne 0) {
    throw "International established FA market normalization tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $NoMinorDemandClassifyTestExe `
    $NoMinorDemandClassifyTestSrc `
    (Join-Path $Root "src\foreign\signability\no_minor_demand\submit_offer_probe_no_minor_demand_classify.c")
if ($LASTEXITCODE -ne 0) {
    throw "No-minor demand classification test build failed"
}

& $NoMinorDemandClassifyTestExe
if ($LASTEXITCODE -ne 0) {
    throw "No-minor demand classification tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $ForeignNoMinorContractRepairTestExe `
    $ForeignNoMinorContractRepairTestSrc `
    (Join-Path $Root "src\foreign\no_minor_contracts\repair\foreign_no_minor_contract_repair_mutation.c") `
    (Join-Path $Root "src\team\assignment\roster_arrays\team_roster_arrays.c")
if ($LASTEXITCODE -ne 0) {
    throw "Foreign no-minor contract repair test build failed"
}

& $ForeignNoMinorContractRepairTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Foreign no-minor contract repair tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $ForeignInjuryExistingReplacementsTestExe `
    $ForeignInjuryExistingReplacementsTestSrc `
    (Join-Path $Root "src\foreign\injury\scanner\lifecycle\foreign_injury_existing_replacements.c")
if ($LASTEXITCODE -ne 0) {
    throw "Foreign injury existing replacements test build failed"
}

& $ForeignInjuryExistingReplacementsTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Foreign injury existing replacements tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $IntlEstablishedFaObservedPlayersTestExe `
    $IntlEstablishedFaObservedPlayersTestSrc `
    (Join-Path $Root "src\foreign\intl_established_fa_postscan\state\intl_established_fa_postscan_state.c") `
    (Join-Path $Root "src\foreign\intl_established_fa_postscan\state\intl_established_fa_observed_players.c") `
    (Join-Path $Root "src\foreign\intl_established_fa_postscan\visibility\intl_established_fa_visibility.c")
if ($LASTEXITCODE -ne 0) {
    throw "International established FA observed player test build failed"
}

& $IntlEstablishedFaObservedPlayersTestExe
if ($LASTEXITCODE -ne 0) {
    throw "International established FA observed player tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $TeamLookupPlayerVectorCacheTestExe `
    $TeamLookupPlayerVectorCacheTestSrc `
    (Join-Path $Root "src\team\lookup\team_lookup.c")
if ($LASTEXITCODE -ne 0) {
    throw "Team lookup player vector cache test build failed"
}

& $TeamLookupPlayerVectorCacheTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Team lookup player vector cache tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $IntlEstablishedFaEventTimingTestExe `
    $IntlEstablishedFaEventTimingTestSrc `
    (Join-Path $Root "src\foreign\intl_established_fa_postscan\timing\intl_established_fa_event_timing.c")
if ($LASTEXITCODE -ne 0) {
    throw "International established FA event timing test build failed"
}

& $IntlEstablishedFaEventTimingTestExe
if ($LASTEXITCODE -ne 0) {
    throw "International established FA event timing tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $AsianGamesHandlerSaveContextTestExe `
    $AsianGamesHandlerSaveContextTestSrc `
    (Join-Path $Root "src\custom_events\asian_games_news\handlers\handlers.c") `
    (Join-Path $Root "src\custom_events\runtime\state\custom_event_state.c") `
    (Join-Path $Root "src\custom_events\asian_games\state\asian_games_state.c")
if ($LASTEXITCODE -ne 0) {
    throw "Asian Games handler save-context test build failed"
}

& $AsianGamesHandlerSaveContextTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Asian Games handler save-context tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $AsianGamesPlayerEligibilityTestExe `
    $AsianGamesPlayerEligibilityTestSrc `
    (Join-Path $Root "src\custom_events\asian_games\player_eval\asian_games_player_eligibility.c")
if ($LASTEXITCODE -ne 0) {
    throw "Asian Games player eligibility test build failed"
}

& $AsianGamesPlayerEligibilityTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Asian Games player eligibility tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $AsianGamesRestrictedMaintenancePolicyTestExe `
    $AsianGamesRestrictedMaintenancePolicyTestSrc `
    (Join-Path $Root "src\custom_events\asian_games_lifecycle\maintenance\asian_games_lifecycle_maintenance_policy.c") `
    (Join-Path $Root "src\core\dates\core_text_date.c")
if ($LASTEXITCODE -ne 0) {
    throw "Asian Games restricted maintenance policy test build failed"
}

& $AsianGamesRestrictedMaintenancePolicyTestExe
if ($LASTEXITCODE -ne 0) {
    throw "Asian Games restricted maintenance policy tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $FaDeclarationRepairTestExe `
    $FaDeclarationRepairTestSrc `
    (Join-Path $Root "src\fa_declaration\repair\fa_declaration_repair.c") `
    (Join-Path $Root "src\core\csv\core_csv.c")
if ($LASTEXITCODE -ne 0) {
    throw "FA declaration repair test build failed"
}

& $FaDeclarationRepairTestExe
if ($LASTEXITCODE -ne 0) {
    throw "FA declaration repair tests failed"
}

& $Gcc -O0 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8 `
    -I $Root `
    -I (Join-Path $Root "src") `
    -o $FaDeclarationContractTestExe `
    $FaDeclarationContractTestSrc `
    (Join-Path $Root "src\fa_declaration\decision\fa_declaration_decision.c") `
    (Join-Path $Root "src\fa_declaration\decision\scoring\fa_declaration_decision_score.c")
if ($LASTEXITCODE -ne 0) {
    throw "FA declaration contract test build failed"
}

& $FaDeclarationContractTestExe
if ($LASTEXITCODE -ne 0) {
    throw "FA declaration contract tests failed"
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
    (Join-Path $Root "src\core\dates\boundary\current_date_boundary.c") `
    (Join-Path $Root "src\core\dates\tick\current_date_tick_capture.c") `
    (Join-Path $Root "src\core\dates\tick\capture\current_date_tick_consumer.c") `
    (Join-Path $Root "src\core\dates\tick\capture\current_date_tick_latest.c") `
    (Join-Path $Root "src\core\dates\tick\capture\current_date_tick_sync_consumers.c") `
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
    (Join-Path $Root "src\amateur_player_quality\reputation\amateur_reputation_balance.c") `
    (Join-Path $Root "src\patch_helpers\bytes\patch_bytes.c") `
    (Join-Path $Root "src\fa_filing\fa_filing_parts\fa_filing_csv_parse.c") `
    (Join-Path $Root "src\fa_salary_snapshot\csv\salary_snapshot_csv_parse.c") `
    (Join-Path $Root "src\core\files\atomic\core_atomic_file.c") `
    (Join-Path $Root "src\core\files\save_paths\platform\windows_path.c") `
    (Join-Path $Root "src\core\logging\event\log_event.c") `
    (Join-Path $Root "src\core\logging\rule_audit.c") `
    (Join-Path $Root "src\core\policy\core_policy.c") `
    (Join-Path $Root "src\core\league_roles\kbo_league_roles.c") `
    (Join-Path $Root "src\hotkey_window\support\assets\nations\ui_nation_table.c") `
    (Join-Path $Root "src\military_service\players\loans\military_native_loan.c") `
    (Join-Path $Root "src\team\assignment\roster_arrays\team_roster_arrays.c") `
    (Join-Path $Root "src\team\assignment\org_query\team_org_assignment_query.c") `
    (Join-Path $Root "src\foreign\common\policy\foreign_player_policy.c") `
    (Join-Path $Root "src\foreign\common\player_eval\foreign_waiver_asian_quota_nations.c") `
    (Join-Path $Root "src\foreign\common\player_eval\foreign_waiver_player_eval.c") `
    (Join-Path $Root "src\foreign\quota\counts\org\foreign_quota_count_state.c") `
    (Join-Path $Root "src\foreign\quota\counts\org\foreign_quota_count_cache.c") `
    (Join-Path $Root "src\foreign\quota\counts\org\foreign_quota_count_snapshot.c") `
    (Join-Path $Root "src\foreign\quota\counts\foreign_quota_counts.c") `
    (Join-Path $Root "src\foreign\injury\state\foreign_injury_duration_text.c") `
    (Join-Path $Root "src\foreign\injury\state\foreign_injury_state.c") `
    (Join-Path $Root "src\foreign\injury\season\foreign_injury_offseason_reset.c") `
    (Join-Path $Root "src\team\independent_acquisition\ai\independent_acquisition_score.c") `
    (Join-Path $Root "src\custom_events\asian_games\policy\asian_games_roster_policy.c") `
    (Join-Path $Root "src\amateur_player_quality\assignment\policy\amateur_assignment_policy.c")
if ($LASTEXITCODE -ne 0) {
    throw "Build failed"
}

& $TestExe
if ($LASTEXITCODE -ne 0) {
    throw "Tests failed"
}

Write-Host "All native tests passed."
