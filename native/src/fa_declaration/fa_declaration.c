#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fa_declaration.h"
#include "fa_declaration_internal.h"
#include "news/fa_declaration_news.h"
#include "sql/fa_declaration_sql_store.h"
#include "../bootstrap/abi/ootp_offsets.h"
#include "../core/core_flags/api/flags_api.h"
#include "../core/core_league_context_parts/api/league_context_lookup.h"
#include "../core/dates/constants/kbo_date_constants.h"
#include "../core/league_roles/kbo_league_roles.h"
#include "../core/logging/core_log.h"
#include "../core/logging/rule_audit.h"
#include "../custom_events/runtime/dates/custom_event_dates.h"
#include "../fa_filing/fa_filing.h"
#include "../fa_filing/fa_filing_parts/fa_filing_csv_parse.h"
#include "../fa_filing/fa_filing_parts/fa_filing_csv_write_helpers.h"
#include "../fa_market_classification/api/fa_market_classification.h"
#include "../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../foreign/common/dates/foreign_waiver_date.h"
#include "../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/lookup/team_lookup.h"
#include "../team/names/team_name_cache.h"

int kbo_get_fa_declaration_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    return kbo_fa_declaration_sql_path(out, out_size);
}

int kbo_fa_declaration_find_latest_decision(
    uint32_t player_id,
    uint32_t season,
    KboFaDeclarationDecision* out_decision)
{
    if (out_decision != NULL) {
        memset(out_decision, 0, sizeof(*out_decision));
    }
    if (player_id == 0u || out_decision == NULL) {
        return 0;
    }

    return kbo_fa_declaration_sql_find_latest_decision(player_id, season, out_decision);
}

uint32_t kbo_fa_declaration_retained_contract_season(uint32_t declaration_season)
{
    if (declaration_season < KBO_SEASON_YEAR_MIN || declaration_season >= KBO_SIM_YEAR_MAX) {
        return declaration_season;
    }
    return declaration_season + 1u;
}

int kbo_handle_fa_declaration_event(uint32_t event_yyyymmdd, const char* source)
{
    if (!kbo_fix_enabled()) {
        return 0;
    }
    if (event_yyyymmdd == 0u) {
        return -1;
    }

    uint32_t season = event_yyyymmdd / 10000u;
    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        league_id = kbo_league_role_main_league_id();
    }

    KboFaDeclarationCandidate* candidates = (KboFaDeclarationCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_DECLARATION_MAX * sizeof(KboFaDeclarationCandidate));
    KboFaMarketClassification* market_rows = (KboFaMarketClassification*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_CLASSIFICATION_MAX * sizeof(KboFaMarketClassification));
    KboFaSalarySnapshotGrade* salary_grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (candidates == NULL || market_rows == NULL || salary_grades == NULL) {
        if (candidates != NULL) { HeapFree(GetProcessHeap(), 0, candidates); }
        if (market_rows != NULL) { HeapFree(GetProcessHeap(), 0, market_rows); }
        if (salary_grades != NULL) { HeapFree(GetProcessHeap(), 0, salary_grades); }
        kbo_log_runtime_line("KBO FA declaration event skipped reason=allocation_failed");
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", event_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "season", season);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_rule_audit_emit_fields(
                "fa.declaration.event",
                "skip",
                "allocation_failed",
                source != NULL ? source : "fa_declaration_event",
                &audit_fields);
        } while (0);
        return -1;
    }

    char ignored_path[MAX_PATH] = {0};
    int grade_count = kbo_fa_salary_snapshot_load_grade_rows(
        season,
        salary_grades,
        KBO_FA_SALARY_SNAPSHOT_GRADE_MAX,
        ignored_path,
        sizeof(ignored_path));

    KboFaMarketScanSummary market_summary;
    memset(&market_summary, 0, sizeof(market_summary));
    int market_count = kbo_collect_fa_market_classifications(
        league_id,
        market_rows,
        KBO_FA_MARKET_CLASSIFICATION_MAX,
        &market_summary,
        0,
        "fa_declaration_event");

    int candidate_count = 0;
    int market_added = 0;
    for (int i = 0; i < market_count && candidate_count < KBO_FA_DECLARATION_MAX; i++) {
        market_added += kbo_fa_declaration_add_market_candidate(
            &market_rows[i],
            event_yyyymmdd,
            season,
            league_id,
            salary_grades,
            grade_count,
            candidates,
            &candidate_count);
    }

    int active_scanned = 0;
    int active_added = kbo_fa_declaration_collect_active_fallback(
        event_yyyymmdd,
        season,
        league_id,
        salary_grades,
        grade_count,
        candidates,
        &candidate_count,
        &active_scanned);
    if (active_added < 0 && market_summary.scanned == 0 && market_count == 0) {
        HeapFree(GetProcessHeap(), 0, candidates);
        HeapFree(GetProcessHeap(), 0, market_rows);
        HeapFree(GetProcessHeap(), 0, salary_grades);
        kbo_log_runtimef(
            "KBO FA declaration event deferred source=%s date=%u reason=no_player_vector",
            source != NULL ? source : "",
            event_yyyymmdd);
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", event_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "season", season);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_i32(&audit_fields, "market_rows", market_count);
            kbo_rule_audit_emit_fields(
                "fa.declaration.event",
                "defer",
                "player_vector_unavailable",
                source != NULL ? source : "fa_declaration_event",
                &audit_fields);
        } while (0);
        return -1;
    }
    if (active_added < 0) {
        active_added = 0;
    }

    int declared = 0;
    int deferred = 0;
    int deferred_retry = 0;
    int deferred_no_market = 0;
    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i].declared) {
            declared++;
        } else {
            deferred++;
            if (strstr(candidates[i].decision_reason, "retry_after_down_year") != NULL) {
                deferred_retry++;
            } else if (strstr(candidates[i].decision_reason, "no_market_stay_original") != NULL) {
                deferred_no_market++;
            }
        }
    }

    char csv_path[MAX_PATH] = {0};
    int wrote = kbo_fa_declaration_append_csv(
        candidates,
        candidate_count,
        source != NULL ? source : "fa_declaration_event",
        csv_path,
        sizeof(csv_path));
    if (!wrote) {
        HeapFree(GetProcessHeap(), 0, candidates);
        HeapFree(GetProcessHeap(), 0, market_rows);
        HeapFree(GetProcessHeap(), 0, salary_grades);
                do {
            KboLogFields audit_fields;
            kbo_log_fields_init(&audit_fields);
            kbo_log_field_u32(&audit_fields, "date", event_yyyymmdd);
            kbo_log_field_u32(&audit_fields, "season", season);
            kbo_log_field_u32(&audit_fields, "league_id", league_id);
            kbo_log_field_i32(&audit_fields, "candidates", candidate_count);
            kbo_log_field_i32(&audit_fields, "declared", declared);
            kbo_log_field_i32(&audit_fields, "deferred", deferred);
            kbo_rule_audit_emit_fields(
                "fa.declaration.event",
                "fail",
                "csv_write_failed",
                source != NULL ? source : "fa_declaration_event",
                &audit_fields);
        } while (0);
        return -1;
    }

    int deferred_arbitration_repaired = kbo_fa_declaration_repair_retained_contracts_for_season(
        season,
        source != NULL ? source : "fa_declaration_event");

    int detail_logs = 0;
    for (int i = 0; i < candidate_count && detail_logs < 40; i++) {
        if (!candidates[i].declared) {
            continue;
        }
        kbo_log_runtimef(
            "KBO FA declaration decided player=%u name=%s team=%u case=%s grade=%s score=%d threshold=%d reason=%s",
            candidates[i].player_id,
            candidates[i].player_name,
            candidates[i].team_id,
            candidates[i].case_label,
            candidates[i].grade,
            candidates[i].score,
            candidates[i].threshold,
            candidates[i].decision_reason);
        detail_logs++;
    }

    int deferred_logs = 0;
    for (int i = 0; i < candidate_count && deferred_logs < 40; i++) {
        if (candidates[i].declared) {
            continue;
        }
        kbo_log_runtimef(
            "KBO FA declaration deferred player=%u name=%s team=%u case=%s grade=%s score=%d threshold=%d reason=%s",
            candidates[i].player_id,
            candidates[i].player_name,
            candidates[i].team_id,
            candidates[i].case_label,
            candidates[i].grade,
            candidates[i].score,
            candidates[i].threshold,
            candidates[i].decision_reason);
        deferred_logs++;
    }

    kbo_log_runtimef(
        "KBO FA declaration event source=%s date=%u season=%u league=%u market_rows=%d market_candidates=%d active_scanned=%d active_candidates=%d candidates=%d declared=%d deferred=%d retry=%d no_market=%d grades=%d deferred_arbitration_repaired=%d csv=%s",
        source != NULL ? source : "",
        event_yyyymmdd,
        season,
        league_id,
        market_count,
        market_added,
        active_scanned,
        active_added,
        candidate_count,
        declared,
        deferred,
        deferred_retry,
        deferred_no_market,
        grade_count,
        deferred_arbitration_repaired,
        csv_path);
        do {
        KboLogFields audit_fields;
        kbo_log_fields_init(&audit_fields);
        kbo_log_field_u32(&audit_fields, "date", event_yyyymmdd);
        kbo_log_field_u32(&audit_fields, "season", season);
        kbo_log_field_u32(&audit_fields, "league_id", league_id);
        kbo_log_field_i32(&audit_fields, "market_rows", market_count);
        kbo_log_field_i32(&audit_fields, "market_candidates", market_added);
        kbo_log_field_i32(&audit_fields, "active_scanned", active_scanned);
        kbo_log_field_i32(&audit_fields, "active_candidates", active_added);
        kbo_log_field_i32(&audit_fields, "candidates", candidate_count);
        kbo_log_field_i32(&audit_fields, "declared", declared);
        kbo_log_field_i32(&audit_fields, "deferred", deferred);
        kbo_log_field_i32(&audit_fields, "retry", deferred_retry);
        kbo_log_field_i32(&audit_fields, "no_market", deferred_no_market);
        kbo_log_field_i32(&audit_fields, "grades", grade_count);
        kbo_log_field_i32(&audit_fields, "deferred_arbitration_repaired", deferred_arbitration_repaired);
        kbo_rule_audit_emit_fields(
            "fa.declaration.event",
            "record_decisions",
            "declaration_window_processed",
            source != NULL ? source : "fa_declaration_event",
            &audit_fields);
    } while (0);

    uint32_t news_yyyymmdd = kbo_custom_event_effective_news_date(event_yyyymmdd);
    kbo_emit_fa_declaration_summary_news(
        news_yyyymmdd,
        season,
        league_id,
        candidates,
        candidate_count,
        declared,
        deferred,
        deferred_retry,
        deferred_no_market,
        source != NULL ? source : "fa_declaration_event");
    kbo_emit_fa_declaration_retry_news(
        news_yyyymmdd,
        season,
        league_id,
        candidates,
        candidate_count,
        deferred_retry,
        source != NULL ? source : "fa_declaration_event");

    HeapFree(GetProcessHeap(), 0, candidates);
    HeapFree(GetProcessHeap(), 0, market_rows);
    HeapFree(GetProcessHeap(), 0, salary_grades);
    return 1;
}
