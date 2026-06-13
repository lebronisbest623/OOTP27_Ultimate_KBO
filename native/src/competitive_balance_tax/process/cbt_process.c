#include "../internal/cbt_internal.h"

#include "../audit/cbt_rule_audit.h"
#include "../draft/order/cbt_draft_order_penalty.h"
#include "../draft/penalty/cbt_draft_penalty.h"
#include "payroll/cbt_payroll_compute.h"
#include "../../hotkey_window/api/hotkey_window_refresh.h"
#include "../../core/dates/constants/kbo_date_constants.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"

int kbo_process_competitive_balance_tax(uint32_t season, const char* source)
{
    return kbo_process_competitive_balance_tax_for_date(season, 0u, source);
}

int kbo_process_competitive_balance_tax_for_date(uint32_t season, uint32_t news_yyyymmdd, const char* source)
{
    if (source == NULL || strcmp(source, "cbt_announcement_event") != 0) {
        kbo_cbt_audit_process("skip", "announcement_event_required", source, season, news_yyyymmdd, 0, 0, 0, 0, 0u, 0, 0);
        kbo_log_runtimef(
            "KBO CBT skipped season=%u source=%s reason=announcement_event_required",
            season,
            source != NULL ? source : "");
        return 0;
    }

    if (season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        kbo_cbt_audit_process("skip", "season_out_of_range", source, season, news_yyyymmdd, 0, 0, 0, 0, 0u, 0, 0);
        return 0;
    }

    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_COMPETITIVE_BALANCE_TAX_FILE)) {
        kbo_cbt_audit_process("skip", "flag_disabled", source, season, news_yyyymmdd, 0, 0, 0, 0, 0u, 0, 0);
        kbo_log_runtimef("KBO CBT skipped season=%u source=%s reason=flag_disabled", season, source != NULL ? source : "");
        return 1;
    }

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);
    if (!rules.enabled) {
        kbo_cbt_audit_process("skip", "rules_disabled", source, season, news_yyyymmdd, 0, 0, 0, 0, 0u, 0, 0);
        kbo_log_runtimef("KBO CBT skipped season=%u source=%s reason=disabled", season, source != NULL ? source : "");
        return 1;
    }

    int32_t threshold = kbo_cbt_get_threshold(&rules, season);
    if (threshold <= 0) {
        kbo_cbt_audit_process("skip", "threshold_unavailable", source, season, news_yyyymmdd, threshold, 0, 0, 0, 0u, 0, 0);
        kbo_log_runtimef("KBO CBT skipped season=%u source=%s reason=no_threshold", season, source != NULL ? source : "");
        return 0;
    }

    /* Load salary snapshot for this season */
    KboFaSalarySnapshotGrade* grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (grades == NULL) {
        kbo_cbt_audit_process("fail", "salary_snapshot_alloc_failed", source, season, news_yyyymmdd, threshold, 0, 0, 0, 0u, 0, 0);
        return 0;
    }

    int grade_count = kbo_fa_salary_snapshot_load_grade_rows(season, grades, KBO_FA_SALARY_SNAPSHOT_GRADE_MAX, NULL, 0);
    if (grade_count <= 0) {
        kbo_cbt_audit_process("skip", "salary_snapshot_empty", source, season, news_yyyymmdd, threshold, grade_count, 0, 0, 0u, 0, 0);
        kbo_log_runtimef("KBO CBT skipped season=%u source=%s reason=no_salary_data", season, source != NULL ? source : "");
        HeapFree(GetProcessHeap(), 0, grades);
        return 0;
    }

    /* Compute per-team domestic payroll (top N) */
    KboCbtTeamPayroll* teams = (KboCbtTeamPayroll*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_CBT_TEAM_MAX * sizeof(KboCbtTeamPayroll));
    if (teams == NULL) {
        kbo_cbt_audit_process("fail", "team_payroll_alloc_failed", source, season, news_yyyymmdd, threshold, grade_count, 0, 0, 0u, 0, 0);
        kbo_log_runtimef("KBO CBT skipped season=%u source=%s reason=team_alloc_failed", season, source != NULL ? source : "");
        HeapFree(GetProcessHeap(), 0, grades);
        return 0;
    }

    int team_count = 0;
    kbo_cbt_exception_auto_designate_missing(season, source != NULL ? source : "cbt_process");
    KboCbtExceptionDesignation exceptions[KBO_CBT_EXCEPTION_MAX];
    int exception_count = kbo_cbt_exception_load_designations(exceptions, KBO_CBT_EXCEPTION_MAX);
    kbo_cbt_compute_team_payrolls(grades, grade_count, season, exceptions, exception_count, rules.top_player_count, teams, &team_count);
    HeapFree(GetProcessHeap(), 0, grades);

    if (team_count == 0) {
        kbo_cbt_audit_process("skip", "no_teams", source, season, news_yyyymmdd, threshold, grade_count, team_count, exception_count, 0u, 0, 0);
        kbo_log_runtimef("KBO CBT skipped season=%u source=%s reason=no_teams", season, source != NULL ? source : "");
        HeapFree(GetProcessHeap(), 0, teams);
        return 0;
    }

    /* Load existing records to check consecutive history */
    KboCbtRecord* records = (KboCbtRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_CBT_RECORDS_MAX * sizeof(KboCbtRecord));
    if (records == NULL) {
        kbo_cbt_audit_process("fail", "records_alloc_failed", source, season, news_yyyymmdd, threshold, grade_count, team_count, exception_count, 0u, 0, 0);
        HeapFree(GetProcessHeap(), 0, teams);
        return 0;
    }
    int record_count = kbo_cbt_load_records(records, KBO_CBT_RECORDS_MAX, NULL, 0);

    uint32_t year = season;
    uint32_t month = 4u;
    uint32_t day = 1u;
    kbo_cbt_news_date(season, news_yyyymmdd, &year, &month, &day);
    uint32_t league_id = kbo_resolve_kbo_league_id();

    int new_violations = 0;
    for (int i = 0; i < team_count; i++) {
        uint32_t team_id = teams[i].team_id;
        int32_t payroll = teams[i].top_n_sum;

        int32_t overage = payroll - threshold;
        uint32_t prior_consecutive = kbo_cbt_get_consecutive_count(records, record_count, team_id, season);
        uint32_t consecutive_count = overage > 0 ? prior_consecutive + 1u : 0u;
        uint32_t tax_rate = kbo_cbt_get_tax_rate(&rules, consecutive_count);
        int32_t tax_amount = overage > 0
            ? (int32_t)((int64_t)overage * (int32_t)tax_rate / 100LL)
            : 0;

        int existing_index = kbo_cbt_find_record(records, record_count, season, team_id);
        if (existing_index >= 0) {
            KboCbtRecord* existing = &records[existing_index];
            int32_t normalized_overage = overage > 0 ? overage : 0;
            if (existing->payroll != payroll
                    || existing->threshold != threshold
                    || existing->overage != normalized_overage
                    || existing->tax_rate_pct != tax_rate
                    || existing->tax_amount != tax_amount
                    || existing->consecutive_count != consecutive_count) {
                kbo_log_runtimef(
                    "KBO CBT record reconciled season=%u team=%u payroll=%d->%d threshold=%d->%d overage=%d->%d rate=%u->%u tax=%d->%d consecutive=%u->%u source=%s",
                    season,
                    team_id,
                    existing->payroll,
                    payroll,
                    existing->threshold,
                    threshold,
                    existing->overage,
                    normalized_overage,
                    existing->tax_rate_pct,
                    tax_rate,
                    existing->tax_amount,
                    tax_amount,
                    existing->consecutive_count,
                    consecutive_count,
                    source != NULL ? source : "");
                existing->payroll = payroll;
                existing->threshold = threshold;
                existing->overage = normalized_overage;
                existing->tax_rate_pct = tax_rate;
                existing->tax_amount = tax_amount;
                existing->consecutive_count = consecutive_count;
                kbo_cbt_copy_team_name(team_id, existing->team_name, sizeof(existing->team_name));
            }
            int draft_penalty = overage > 0 && consecutive_count >= rules.draft_penalty_min_consecutive;
            kbo_cbt_audit_team(
                "skip",
                "record_already_exists",
                source,
                season,
                team_id,
                payroll,
                threshold,
                normalized_overage,
                tax_rate,
                tax_amount,
                consecutive_count,
                draft_penalty,
                teams[i].exception_credit);
            continue;
        }

        KboCbtRecord rec = {0};
        rec.season           = season;
        rec.team_id          = team_id;
        rec.payroll          = payroll;
        rec.threshold        = threshold;
        rec.overage          = overage > 0 ? overage : 0;
        rec.tax_rate_pct     = tax_rate;
        rec.tax_amount       = tax_amount;
        rec.consecutive_count = consecutive_count;
        rec.processed_date   = year * 10000u + month * 100u + day;
        kbo_cbt_copy_team_name(team_id, rec.team_name, sizeof(rec.team_name));

        if (record_count < KBO_CBT_RECORDS_MAX) {
            records[record_count++] = rec;
        }

        if (overage > 0) {
            new_violations++;
            int draft_penalty = (int)consecutive_count >= (int)rules.draft_penalty_min_consecutive;
            kbo_cbt_audit_team(
                "tax",
                "threshold_exceeded",
                source,
                season,
                team_id,
                payroll,
                threshold,
                overage,
                tax_rate,
                tax_amount,
                consecutive_count,
                draft_penalty,
                teams[i].exception_credit);
            kbo_log_runtimef(
                "KBO CBT violation season=%u team=%u payroll=%d threshold=%d overage=%d rate=%u%% tax=%d consecutive=%u draft_penalty=%d exception_credit=%d source=%s",
                season, team_id, payroll, threshold, overage,
                tax_rate, tax_amount, consecutive_count, draft_penalty,
                teams[i].exception_credit,
                source != NULL ? source : "");

            if (league_id != 0u) {
                kbo_cbt_insert_violation_news_v2(league_id, year, month, day, &rec, &rules);
            }
        } else {
            kbo_cbt_audit_team("clean", "threshold_not_exceeded", source, season, team_id, payroll, threshold, 0, 0u, 0, 0u, 0, teams[i].exception_credit);
            kbo_log_runtimef(
                "KBO CBT clean season=%u team=%u payroll=%d threshold=%d exception_credit=%d source=%s",
                season, team_id, payroll, threshold,
                teams[i].exception_credit,
                source != NULL ? source : "");
        }
    }

    int records_saved = kbo_cbt_save_records(records, record_count);
    if (records_saved) {
        kbo_cbt_draft_penalty_cache_clear();
    }
    int draft_order_moves = 0;
    if (league_id != 0u) {
        draft_order_moves = kbo_cbt_apply_primary_league_draft_order_penalties(
            league_id,
            "cbt_process_primary_league_state");
    }
    if (draft_order_moves <= 0) {
        draft_order_moves = kbo_cbt_apply_pending_draft_order_penalties("cbt_process_post_records");
    }
    if (draft_order_moves > 0) {
        kbo_log_runtimef(
            "KBO CBT draft order penalties applied after record save season=%u moves=%d source=%s",
            season,
            draft_order_moves,
            source != NULL ? source : "");
    }
    int summary_ready = 0;
    if (league_id != 0u) {
        char summary_marker[64] = {0};
        snprintf(summary_marker, sizeof(summary_marker), "summary|%u|%u", season, league_id);
        if (kbo_cbt_news_marker_exists(summary_marker)) {
            summary_ready = 1;
            kbo_cbt_audit_summary_news("skip", "marker_exists", source, season, league_id, team_count, new_violations);
            kbo_log_runtimef(
                "KBO CBT opening-day news skipped season=%u league_id=%u reason=summary_marker_exists source=%s",
                season,
                league_id,
                source != NULL ? source : "");
        } else {
            int summary_created = kbo_cbt_insert_opening_day_summary_news(
                league_id,
                year,
                month,
                day,
                season,
                records,
                record_count,
                team_count,
                &rules);
            kbo_cbt_audit_summary_news(
                summary_created ? "emit_news" : "skip",
                summary_created ? "summary_created" : "news_insert_failed",
                source,
                season,
                league_id,
                team_count,
                new_violations);
            if (summary_created) {
                summary_ready = 1;
                kbo_cbt_news_persist_marker(summary_marker, source);
            }
        }
    }
    kbo_request_hotkey_window_refresh("competitive_balance_tax_processed");
    int success = records_saved && (league_id == 0u || summary_ready);

    if (success) {
        kbo_log_runtimef(
            "KBO CBT processed season=%u teams=%d violations=%d threshold=%d source=%s",
            season, team_count, new_violations, threshold,
            source != NULL ? source : "");
        kbo_cbt_audit_process("processed", "completed", source, season, news_yyyymmdd, threshold, grade_count, team_count, exception_count, league_id, new_violations, record_count);
    } else {
        kbo_log_runtimef(
            "KBO CBT completion deferred season=%u records_saved=%d summary_ready=%d league_id=%u source=%s",
            season,
            records_saved,
            summary_ready,
            league_id,
            source != NULL ? source : "");
        kbo_cbt_audit_process("deferred", "outputs_incomplete", source, season, news_yyyymmdd, threshold, grade_count, team_count, exception_count, league_id, new_violations, record_count);
    }
    HeapFree(GetProcessHeap(), 0, records);
    HeapFree(GetProcessHeap(), 0, teams);
    return success;
}
