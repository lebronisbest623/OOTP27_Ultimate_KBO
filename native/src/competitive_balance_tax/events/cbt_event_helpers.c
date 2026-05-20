#include "cbt_event_helpers.h"
#include "cbt_events.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/logging/core_log.h"
#include "../../custom_events/runtime/ledger/custom_event_ledger.h"
#include "../../custom_events/runtime/markers/custom_event_markers.h"
#include "../../custom_events/runtime/runner/custom_event_runner.h"
#include "../../fa_salary_snapshot/capture/salary_snapshot_write_capture.h"
#include "../../fa_salary_snapshot/grading/salary_snapshot_grade_rows.h"
#include "../exceptions/cbt_exceptions.h"
#include "../internal/cbt_internal.h"
#include "../records/cbt_records.h"
#include "../rules/cbt_rules.h"
#include "../../core/dates/constants/kbo_date_constants.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"

static volatile LONG64 g_kbo_cbt_last_no_date_log_ms = 0;

int kbo_cbt_should_log_no_date(void)
{
    ULONGLONG now = GetTickCount64();
    LONG64 last = InterlockedCompareExchange64(&g_kbo_cbt_last_no_date_log_ms, 0, 0);
    if (last > 0 && now >= (ULONGLONG)last && now - (ULONGLONG)last < 30000ull) {
        return 0;
    }
    return InterlockedCompareExchange64(&g_kbo_cbt_last_no_date_log_ms, (LONG64)now, last) == last;
}

static int kbo_cbt_salary_snapshot_has_rows(uint32_t season)
{
    KboFaSalarySnapshotGrade grade;
    memset(&grade, 0, sizeof(grade));
    return kbo_fa_salary_snapshot_load_grade_rows(season, &grade, 1, NULL, 0) > 0;
}

int kbo_cbt_ensure_salary_snapshot_rows(
    uint32_t season,
    uint32_t event_yyyymmdd,
    const char* source)
{
    if (kbo_cbt_salary_snapshot_has_rows(season)) {
        return 1;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (league_id == 0u) {
        kbo_log_runtimef(
            "KBO CBT salary snapshot ensure deferred source=%s season=%u reason=league_id_unavailable",
            source != NULL ? source : "",
            season);
        return 0;
    }

    uint32_t opening_day = 0u;
    if (!kbo_cbt_exception_resolve_opening_day(season, &opening_day)) {
        kbo_log_runtimef(
            "KBO CBT salary snapshot ensure deferred source=%s season=%u league=%u reason=opening_day_unavailable",
            source != NULL ? source : "",
            season,
            league_id);
        return 0;
    }

    if (opening_day / 10000u != season
            || event_yyyymmdd == 0u
            || event_yyyymmdd < opening_day) {
        kbo_log_runtimef(
            "KBO CBT salary snapshot ensure deferred source=%s season=%u event_date=%u opening_day=%u reason=outside_opening_window",
            source != NULL ? source : "",
            season,
            event_yyyymmdd,
            opening_day);
        return 0;
    }

    int captured = kbo_capture_fa_salary_opening_day_snapshot(
        source != NULL ? source : "cbt_salary_snapshot_ensure",
        event_yyyymmdd,
        season,
        opening_day,
        league_id);
    if (!captured) {
        return kbo_cbt_salary_snapshot_has_rows(season);
    }
    return kbo_cbt_salary_snapshot_has_rows(season);
}

static int kbo_cbt_exception_designations_have_season(uint32_t season)
{
    KboCbtExceptionDesignation rows[KBO_CBT_EXCEPTION_MAX];
    int count = kbo_cbt_exception_load_designations(rows, KBO_CBT_EXCEPTION_MAX);
    for (int i = 0; i < count; i++) {
        if (rows[i].season == season) {
            return 1;
        }
    }
    return 0;
}

static int kbo_cbt_records_have_season(uint32_t season)
{
    KboCbtRecord* records = (KboCbtRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_CBT_RECORDS_MAX * sizeof(KboCbtRecord));
    if (records == NULL) {
        return 0;
    }
    int count = kbo_cbt_load_records(records, KBO_CBT_RECORDS_MAX, NULL, 0);
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (records[i].season == season) {
            found = 1;
            break;
        }
    }
    HeapFree(GetProcessHeap(), 0, records);
    return found;
}

int kbo_cbt_custom_event_completion_valid(uint32_t league_id, uint32_t event_yyyymmdd, KboCustomEventKind kind)
{
    uint32_t season = event_yyyymmdd / 10000u;
    if (season < KBO_SEASON_YEAR_MIN || season > KBO_SIM_YEAR_MAX) {
        return 1;
    }
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_COMPETITIVE_BALANCE_TAX_FILE)) {
        return 1;
    }
    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);
    if (!rules.enabled) {
        return 1;
    }

    if (kind == KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE) {
        return kbo_cbt_salary_snapshot_has_rows(season)
            && kbo_cbt_exception_designations_have_season(season);
    }
    if (kind == KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT) {
        char summary_marker[64] = {0};
        snprintf(summary_marker, sizeof(summary_marker), "summary|%u|%u", season, league_id);
        return kbo_cbt_records_have_season(season)
            && league_id != 0u
            && kbo_cbt_news_marker_exists(summary_marker);
    }
    return 1;
}

int kbo_process_due_cbt_custom_event(
    uint32_t today,
    uint32_t league_id,
    uint32_t event_date,
    KboCustomEventKind kind,
    const char* title,
    const char* source)
{
    if (event_date == 0u || today == 0u || today < event_date) {
        return 0;
    }
    int completed = kbo_custom_event_processed_marker_exists_for_kind(event_date, kind)
        || kbo_custom_event_ledger_completed(league_id, event_date, kind);
    if (completed && kbo_cbt_custom_event_completion_valid(league_id, event_date, kind)) {
        return 0;
    }
    if (completed) {
        kbo_log_runtimef(
            "KBO CBT due event stale completion ignored source=%s kind=%s event_date=%u today=%u",
            source != NULL ? source : "",
            kbo_custom_event_kind_key(kind),
            event_date,
            today);
    }

    int result = kbo_run_custom_event_by_kind(
        0,
        league_id,
        event_date,
        kind,
        title,
        source);
    if (result > 0) {
        if (result == KBO_CUSTOM_EVENT_RUN_ALREADY_COMPLETED) {
            return 0;
        }
        kbo_log_runtimef(
            "KBO CBT due event handled source=%s kind=%s event_date=%u today=%u result=%d",
            source != NULL ? source : "",
            kbo_custom_event_kind_key(kind),
            event_date,
            today,
            result);
        return 1;
    }

    kbo_log_runtimef(
        "KBO CBT due event deferred source=%s kind=%s event_date=%u today=%u result=%d",
        source != NULL ? source : "",
        kbo_custom_event_kind_key(kind),
        event_date,
        today,
        result);
    return -1;
}
