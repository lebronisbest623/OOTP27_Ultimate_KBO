#include "../../runtime/common/custom_events_common.h"
#include "asian_games_schedule.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/events/core_league_events.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../asian_games_schedule_seed/query/query_helpers.h"
#include "../../runtime/ledger/custom_event_ledger.h"
#include "../../runtime/runner/custom_event_runner.h"

static volatile LONG64 g_kbo_asian_games_schedule_seed_deferred_log_ms = 0;

static int kbo_asian_games_schedule_should_log_seed_deferred(void)
{
    ULONGLONG now = GetTickCount64();
    LONG64 last = InterlockedCompareExchange64(&g_kbo_asian_games_schedule_seed_deferred_log_ms, 0, 0);
    if (last > 0 && now >= (ULONGLONG)last && now - (ULONGLONG)last < 30000ull) {
        return 0;
    }
    return InterlockedCompareExchange64(
        &g_kbo_asian_games_schedule_seed_deferred_log_ms,
        (LONG64)now,
        last) == last;
}

static int kbo_process_due_asian_games_custom_event(
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
    if (kbo_custom_event_processed_marker_exists_for_kind(event_date, kind)
            || kbo_custom_event_ledger_completed(league_id, event_date, kind)) {
        return 0;
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
            "KBO Asian Games due event handled source=%s kind=%s event_date=%u today=%u result=%d",
            source != NULL ? source : "",
            kbo_custom_event_kind_key(kind),
            event_date,
            today,
            result);
        return 1;
    }

    kbo_log_runtimef(
        "KBO Asian Games due event deferred source=%s kind=%s event_date=%u today=%u result=%d",
        source != NULL ? source : "",
        kbo_custom_event_kind_key(kind),
        event_date,
        today,
        result);
    return -1;
}

static int kbo_asian_games_schedule_seed_source_available(void)
{
    char global_path[MAX_PATH] = {0};
    if (kbo_get_global_asian_games_schedule_seed_path(global_path, sizeof(global_path))
            && global_path[0] != '\0') {
        DWORD attrs = GetFileAttributesA(global_path);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0u) {
            return 1;
        }
    }

    char save_path[MAX_PATH] = {0};
    if (kbo_get_save_asian_games_schedule_seed_path(save_path, sizeof(save_path))
            && save_path[0] != '\0') {
        DWORD attrs = GetFileAttributesA(save_path);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0u) {
            return 1;
        }
    }
    return 0;
}

int kbo_schedule_asian_games_custom_events_for_date(uint32_t today, const char* source)
{
    uint32_t year = today / 10000u;
    if (kbo_date_serial(year, (today / 100u) % 100u, today % 100u) == 0u) {
        kbo_log_runtimef(
            "KBO Asian Games schedule skipped source=%s reason=current_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }
    KboAsianGamesScheduleSeed schedule;
    if (!kbo_get_asian_games_schedule_for_year(year, &schedule)) {
        if (!kbo_asian_games_schedule_seed_source_available()) {
            if (kbo_asian_games_schedule_should_log_seed_deferred()) {
                kbo_log_runtimef(
                    "KBO Asian Games schedule deferred source=%s reason=schedule_seed_source_unavailable year=%u today=%u",
                    source != NULL ? source : "",
                    year,
                    today);
            }
            return -1;
        }
        return 0;
    }
    if (!kbo_asian_games_schedule_auto_events_enabled(&schedule)) {
        return 0;
    }
    int year_was_marked_scheduled = g_kbo_asian_games_last_scheduled_year == year;

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        kbo_log_runtimef(
            "KBO Asian Games schedule skipped source=%s reason=league_id_unavailable year=%u",
            source != NULL ? source : "",
            year);
        return -1;
    }

    uint32_t selection_date = schedule.selection_date;
    uint32_t departure_date = schedule.departure_date;
    uint32_t final_date = schedule.final_date;
    if (today > final_date) {
        g_kbo_asian_games_last_scheduled_year = year;
        kbo_log_runtimef(
            "KBO Asian Games schedule skipped source=%s reason=events_past year=%u today=%u final=%u",
            source != NULL ? source : "",
            year,
            today,
            final_date);
        return 0;
    }

    int created_selection = 0;
    int created_departure = 0;
    int created_final = 0;
    int skipped_past_selection = selection_date < today;
    int skipped_past_departure = departure_date < today;
    int skipped_past_final = final_date < today;
    char selection_title[160] = {0};
    char departure_title[160] = {0};
    char final_title[160] = {0};
    if (!kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION, selection_title, sizeof(selection_title))
            || !kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE, departure_title, sizeof(departure_title))
            || !kbo_custom_event_title_for_kind(KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL, final_title, sizeof(final_title))) {
        kbo_log_runtimef(
            "KBO Asian Games schedule skipped source=%s reason=title_unavailable year=%u today=%u",
            source != NULL ? source : "",
            year,
            today);
        return -1;
    }

    int selection_completed = kbo_custom_event_processed_marker_exists_for_kind(
            selection_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION)
        || kbo_custom_event_ledger_completed(
            league_id,
            selection_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION);
    int departure_completed = kbo_custom_event_processed_marker_exists_for_kind(
            departure_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE)
        || kbo_custom_event_ledger_completed(
            league_id,
            departure_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE);
    int final_completed = kbo_custom_event_processed_marker_exists_for_kind(
            final_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL)
        || kbo_custom_event_ledger_completed(
            league_id,
            final_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL);

    int selection_exists = skipped_past_selection || selection_completed || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            selection_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION);
    int departure_exists = skipped_past_departure || departure_completed || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            departure_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE);
    int final_exists = skipped_past_final || final_completed || kbo_custom_event_exists_by_kind_for_date(
            league_id,
            final_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL);

    int direct_processed = 0;
    int direct_deferred = 0;
    int direct_result = kbo_process_due_asian_games_custom_event(
        today,
        league_id,
        selection_date,
        KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION,
        selection_title,
        source);
    if (direct_result > 0) {
        direct_processed = 1;
        selection_exists = 1;
    } else if (direct_result < 0) {
        direct_deferred = 1;
    }
    direct_result = kbo_process_due_asian_games_custom_event(
        today,
        league_id,
        departure_date,
        KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE,
        departure_title,
        source);
    if (direct_result > 0) {
        direct_processed = 1;
        departure_exists = 1;
    } else if (direct_result < 0) {
        direct_deferred = 1;
    }
    direct_result = kbo_process_due_asian_games_custom_event(
        today,
        league_id,
        final_date,
        KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL,
        final_title,
        source);
    if (direct_result > 0) {
        direct_processed = 1;
        final_exists = 1;
    } else if (direct_result < 0) {
        direct_deferred = 1;
    }

    if (year_was_marked_scheduled
            && selection_exists
            && departure_exists
            && final_exists) {
        return direct_deferred ? -1 : direct_processed;
    }
    if (year_was_marked_scheduled) {
        kbo_log_runtimef(
            "KBO Asian Games schedule retry source=%s reason=marked_year_missing_events year=%u today=%u selection_exists=%d departure_exists=%d final_exists=%d",
            source != NULL ? source : "",
            year,
            today,
            selection_exists,
            departure_exists,
            final_exists);
    }

    if (!selection_exists) {
        created_selection = create_kbo_league_event(
            selection_date / 10000u,
            (selection_date / 100u) % 100u,
            selection_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            selection_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }
    if (!departure_exists) {
        created_departure = create_kbo_league_event(
            departure_date / 10000u,
            (departure_date / 100u) % 100u,
            departure_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            departure_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }
    if (!final_exists) {
        created_final = create_kbo_league_event(
            final_date / 10000u,
            (final_date / 100u) % 100u,
            final_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            final_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    selection_completed = selection_completed
        || kbo_custom_event_processed_marker_exists_for_kind(
            selection_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION)
        || kbo_custom_event_ledger_completed(
            league_id,
            selection_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION);
    departure_completed = departure_completed
        || kbo_custom_event_processed_marker_exists_for_kind(
            departure_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE)
        || kbo_custom_event_ledger_completed(
            league_id,
            departure_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE);
    final_completed = final_completed
        || kbo_custom_event_processed_marker_exists_for_kind(
            final_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL)
        || kbo_custom_event_ledger_completed(
            league_id,
            final_date,
            KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL);

    selection_exists = skipped_past_selection || selection_completed || created_selection || kbo_custom_event_exists_by_kind_for_date(
        league_id,
        selection_date,
        KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION);
    departure_exists = skipped_past_departure || departure_completed || created_departure || kbo_custom_event_exists_by_kind_for_date(
        league_id,
        departure_date,
        KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE);
    final_exists = skipped_past_final || final_completed || created_final || kbo_custom_event_exists_by_kind_for_date(
        league_id,
        final_date,
        KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL);
    if (selection_exists && departure_exists && final_exists) {
        g_kbo_asian_games_last_scheduled_year = year;
    }
    kbo_log_runtimef(
        "KBO Asian Games schedule source=%s year=%u today=%u selection=%u departure=%u final=%u created_selection=%d created_departure=%d created_final=%d skipped_past_selection=%d skipped_past_departure=%d skipped_past_final=%d ready=%d",
        source != NULL ? source : "",
        year,
        today,
        selection_date,
        departure_date,
        final_date,
        created_selection,
        created_departure,
        created_final,
        skipped_past_selection,
        skipped_past_departure,
        skipped_past_final,
        selection_exists && departure_exists && final_exists);
    if (!(selection_exists && departure_exists && final_exists)) {
        return -1;
    }
    if (direct_deferred) {
        return -1;
    }
    return created_selection || created_departure || created_final || direct_processed;
}

int kbo_schedule_asian_games_custom_events(const char* source)
{
    uint32_t today = 0u;
    if (!kbo_current_date_tick_latest_published_date(&today)) {
        kbo_log_runtimef(
            "KBO Asian Games schedule skipped source=%s reason=ssot_date_unavailable",
            source != NULL ? source : "");
        return -1;
    }
    return kbo_schedule_asian_games_custom_events_for_date(today, source);
}
