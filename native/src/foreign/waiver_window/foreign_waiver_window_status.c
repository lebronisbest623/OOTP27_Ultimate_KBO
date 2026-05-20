#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/profiling/profiler.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/csv/core_csv.h"
#include "../../core/dates/core_text_date.h"
#include "../../core/dates/tick/current_date_tick_capture.h"
#include "../../core/logging/core_log.h"
#include "../common/dates/foreign_waiver_date.h"
#include "../common/events/foreign_waiver_events.h"
#include "../common/paths/foreign_waiver_paths.h"
#include "../common/policy/foreign_player_policy.h"
#include "../common/policy/foreign_waiver_policy.h"
#include "../waiver_core/api/foreign_waiver_core.h"
#include "events/foreign_waiver_window_events.h"
#include "state/foreign_waiver_window_state.h"
#include "../../core/dates/constants/kbo_date_constants.h"

int kbo_advance_foreign_waiver_window(uint32_t today_yyyymmdd, uint32_t today_serial)
{
    KBO_PROFILE_BEGIN(profile_foreign_waiver_advance);
    static uint32_t last_checked_yyyymmdd = 0u;
    static uint32_t last_checked_result = 0u;
    if (today_yyyymmdd != 0u
            && today_yyyymmdd == last_checked_yyyymmdd
            && last_checked_result != 0u) {
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.cached");
        return 0;
    }
    last_checked_yyyymmdd = today_yyyymmdd;
    last_checked_result = 0u;

    static uint32_t last_logged_evaluate_today = 0u;
    if (last_logged_evaluate_today != today_yyyymmdd) {
        last_logged_evaluate_today = today_yyyymmdd;
        kbo_log_runtimef(
            "foreign waiver auto: evaluate season-end check today=%u serial=%u prev_marker=%u",
            today_yyyymmdd, today_serial,
            g_kbo_foreign_waiver_last_seen_yyyymmdd);
    }

    if (today_yyyymmdd == 0 || today_serial == 0) {
        kbo_log_runtime_line("foreign waiver auto: season-end check skipped - invalid date");
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.invalid_date");
        return 0;
    }

    uint32_t configured_league_id = kbo_resolve_kbo_league_id();
    if (configured_league_id == 0u) {
        kbo_log_runtime_line("foreign waiver auto: missing configured league id; skip season-end evaluation");
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.no_league_id");
        return 0;
    }
    uint32_t window_days = (uint32_t)kbo_foreign_player_policy()->waiver_window_days;

    uint32_t season_end_yyyymmdd = kbo_detect_offseason_starts_event(today_yyyymmdd, configured_league_id);
    if (season_end_yyyymmdd == 0u) {
        uint32_t recovered_marker = g_kbo_foreign_waiver_last_seen_yyyymmdd;
        if (recovered_marker != 0u && recovered_marker <= today_yyyymmdd) {
            uint32_t recovered_serial = kbo_date_serial(
                recovered_marker / 10000u,
                (recovered_marker / 100u) % 100u,
                recovered_marker % 100u);
            uint32_t recovered_end = kbo_add_days_yyyymmdd(recovered_marker, window_days);
            if (recovered_serial != 0u
                    && recovered_end != 0u
                    && today_serial >= recovered_serial
                    && today_serial < recovered_serial + window_days) {
                g_kbo_foreign_waiver_window_start_serial = recovered_serial;
                g_kbo_foreign_waiver_window_end_serial = recovered_serial + window_days;
                g_kbo_foreign_waiver_start_event_date = recovered_marker;
                g_kbo_foreign_waiver_close_event_end_date = recovered_end;
                kbo_write_foreign_waiver_window(recovered_marker, recovered_end, "recovered_recent_offseason_marker");
                kbo_log_runtimef(
                    "foreign waiver auto: recovered missing Offseason starts marker start=%u end=%u today=%u prev_marker=%u",
                    recovered_marker,
                    recovered_end,
                    today_yyyymmdd,
                    g_kbo_foreign_waiver_last_seen_yyyymmdd);
                last_checked_result = 1u;
                KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.recovered_marker");
                return 1;
            }
        }
        static uint32_t last_logged_no_marker_today = 0u;
        if (last_logged_no_marker_today != today_yyyymmdd) {
            last_logged_no_marker_today = today_yyyymmdd;
            kbo_log_runtimef("foreign waiver auto: no Offseason starts event found today=%u", today_yyyymmdd);
        }
        last_checked_result = 2u;
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.no_offseason_marker");
        return 0;
    }
    if (today_yyyymmdd < season_end_yyyymmdd) {
        kbo_log_runtimef(
            "foreign waiver auto: waiting for Offseason starts marker season_end=%u today=%u",
            season_end_yyyymmdd,
            today_yyyymmdd);
        last_checked_result = 2u;
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.waiting_marker");
        return 0;
    }

    uint32_t season_end_serial = kbo_date_serial(
        season_end_yyyymmdd / 10000u,
        (season_end_yyyymmdd / 100u) % 100u,
        season_end_yyyymmdd % 100u);
    if (season_end_serial == 0u) {
        kbo_log_runtimef(
            "foreign waiver auto: invalid Offseason starts serial season_end=%u today=%u",
            season_end_yyyymmdd,
            today_yyyymmdd);
        last_checked_result = 2u;
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.invalid_marker_serial");
        return 0;
    }
    uint32_t window_end_yyyymmdd = kbo_add_days_yyyymmdd(season_end_yyyymmdd, window_days);
    if (window_end_yyyymmdd == 0u) {
        kbo_log_runtimef(
            "foreign waiver auto: invalid Offseason starts window end season_end=%u today=%u",
            season_end_yyyymmdd,
            today_yyyymmdd);
        last_checked_result = 2u;
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.invalid_window_end");
        return 0;
    }
    if (today_serial >= season_end_serial + window_days) {
        static uint32_t last_stale_marker = 0u;
        g_kbo_foreign_waiver_last_seen_yyyymmdd = season_end_yyyymmdd;
        if (last_stale_marker != season_end_yyyymmdd) {
            last_stale_marker = season_end_yyyymmdd;
            kbo_write_foreign_waiver_window(season_end_yyyymmdd, window_end_yyyymmdd, "stale_offseason_marker_preserve_window");
            g_kbo_foreign_waiver_window_start_serial = season_end_serial;
            g_kbo_foreign_waiver_window_end_serial = season_end_serial + window_days;
            g_kbo_foreign_waiver_start_event_date = season_end_yyyymmdd;
            g_kbo_foreign_waiver_close_event_end_date = window_end_yyyymmdd;
            kbo_log_runtimef(
                "foreign waiver auto: skipped stale season-end marker season_end=%u end=%u today=%u reason=window_already_elapsed",
                season_end_yyyymmdd,
                window_end_yyyymmdd,
                today_yyyymmdd);
        }
        last_checked_result = 2u;
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.stale_marker");
        return 0;
    }
    if (g_kbo_foreign_waiver_last_seen_yyyymmdd == season_end_yyyymmdd) {
        kbo_write_foreign_waiver_window(season_end_yyyymmdd, window_end_yyyymmdd, "already_opened_restore_window");
        g_kbo_foreign_waiver_window_start_serial = season_end_serial;
        g_kbo_foreign_waiver_window_end_serial = season_end_serial + window_days;
        g_kbo_foreign_waiver_start_event_date = season_end_yyyymmdd;
        g_kbo_foreign_waiver_close_event_end_date = window_end_yyyymmdd;
        static uint32_t last_logged_already_marker = 0u;
        if (last_logged_already_marker != season_end_yyyymmdd) {
            last_logged_already_marker = season_end_yyyymmdd;
            kbo_log_runtimef(
                "foreign waiver auto: already opened for marker=%u end=%u today=%u",
                season_end_yyyymmdd,
                window_end_yyyymmdd,
                today_yyyymmdd);
        }
        last_checked_result = 2u;
        KBO_PROFILE_END(profile_foreign_waiver_advance, "foreign_waiver.advance.already_opened");
        return 0;
    }

    g_kbo_foreign_waiver_last_seen_yyyymmdd = season_end_yyyymmdd;
    kbo_log_runtimef(
        "foreign waiver auto: season-end marker confirmed season_end=%u today=%u",
        season_end_yyyymmdd,
        today_yyyymmdd);
    int opened = kbo_open_foreign_waiver_window(
        season_end_yyyymmdd,
        season_end_serial,
        "offseason_starts_event");
    last_checked_result = opened ? 1u : 2u;
    KBO_PROFILE_END(profile_foreign_waiver_advance, opened ? "foreign_waiver.advance.opened" : "foreign_waiver.advance.open_failed");
    return opened;
}

int kbo_read_foreign_waiver_window(uint32_t* out_start, uint32_t* out_end)
{
    if (out_start == NULL || out_end == NULL) {
        return 0;
    }
    *out_start = 0;
    *out_end = 0;

    char path[MAX_PATH] = {0};
    if (!get_kbo_foreign_waiver_event_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    char raw[128] = {0};
    DWORD read = 0;
    if (!ReadFile(file, raw, sizeof(raw) - 1, &read, NULL)) {
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    if (read == 0) {
        return 0;
    }

    char fields[2][32];
    int field_count = kbo_csv_read_trimmed_line_fields(raw, (char*)fields, sizeof(fields[0]), 2);
    if (field_count < 2 || fields[0][0] == '#') {
        return 0;
    }

    if (!kbo_parse_yyyymmdd(fields[0], out_start)) {
        return 0;
    }
    if (!kbo_parse_yyyymmdd(fields[1], out_end)) {
        return 0;
    }
    return 1;
}

int kbo_read_foreign_waiver_window_cached(uint32_t today, uint32_t* out_start, uint32_t* out_end)
{
    static uint32_t cached_today = 0u;
    static uint32_t cached_start = 0u;
    static uint32_t cached_end = 0u;
    static int cached_ok = 0;
    static DWORD cached_tick = 0u;

    if (out_start == NULL || out_end == NULL) {
        return 0;
    }
    *out_start = 0u;
    *out_end = 0u;

    DWORD now = GetTickCount();
    if (cached_today != today || cached_tick == 0u || now - cached_tick > 2000u) {
        uint32_t start = 0u;
        uint32_t end = 0u;
        cached_ok = kbo_read_foreign_waiver_window(&start, &end);
        cached_start = start;
        cached_end = end;
        cached_today = today;
        cached_tick = now;
    }

    if (!cached_ok) {
        return 0;
    }
    *out_start = cached_start;
    *out_end = cached_end;
    return 1;
}

int kbo_is_foreign_waiver_negotiation_window_open(void)
{
    if (!kbo_fix_enabled() || !kbo_foreign_waiver_ai_enabled()) {
        return 0;
    }

    uint32_t today = 0;
    if (!kbo_current_date_tick_latest_published_date(&today)) {
        return 0;
    }

    uint32_t today_serial = kbo_date_serial(today / 10000u, (today / 100u) % 100u, today % 100u);
    kbo_advance_foreign_waiver_window(today, today_serial);

    uint32_t start = 0;
    uint32_t end = 0;
    if (!kbo_read_foreign_waiver_window_cached(today, &start, &end)) {
        int open = (g_kbo_foreign_waiver_window_end_serial != 0
                && today_serial >= g_kbo_foreign_waiver_window_start_serial
                && today_serial < g_kbo_foreign_waiver_window_end_serial);
        if (!open && g_kbo_foreign_waiver_window_end_serial != 0
                && today_serial > g_kbo_foreign_waiver_window_end_serial) {
            if (g_kbo_foreign_waiver_last_close_event_end != g_kbo_foreign_waiver_window_end_serial) {
                g_kbo_foreign_waiver_last_close_event_end = g_kbo_foreign_waiver_window_end_serial;
                kbo_log_runtimef(
                    "foreign priority negotiation: event closed today=%u end_serial=%u source=auto detector",
                    today,
                    g_kbo_foreign_waiver_window_end_serial);
            }
        }
        return open;
    }

    if (today >= start && today < end) {
        return 1;
    }
    return 0;
}

int kbo_format_ymd(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size < 11u) {
        return 0;
    }
    uint32_t year = yyyymmdd / 10000u;
    uint32_t month = (yyyymmdd / 100u) % 100u;
    uint32_t day = yyyymmdd % 100u;
    if (year == 0u || month == 0u || day == 0u
            || month > 12u || day > 31u || year < KBO_TEXT_DATE_YEAR_MIN) {
        return 0;
    }
    return (snprintf(out, out_size, "%04u-%02u-%02u", year, month, day) > 0) ? 1 : 0;
}

int kbo_get_foreign_waiver_window_status_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    if (!kbo_foreign_waiver_ai_enabled()) {
        snprintf(out, out_size, "Foreign waiver negotiation: disabled (set enable_foreign_waiver_ai=true in kbo_flags.json)");
        return 1;
    }

    if (!kbo_fix_enabled()) {
        snprintf(out, out_size, "Foreign waiver negotiation: disabled (KBO fix off)");
        return 1;
    }

    uint32_t today = 0;
    if (!kbo_current_date_tick_latest_published_date(&today)) {
        snprintf(out, out_size, "Foreign waiver negotiation: unavailable (failed to read game date)");
        return 1;
    }
    int window_open = kbo_is_foreign_waiver_negotiation_window_open();
    uint32_t file_start = 0;
    uint32_t file_end = 0;
    const char* source = "auto detector";
    if (kbo_read_foreign_waiver_window(&file_start, &file_end)) {
        source = "override file";
    }

    char start_text[16] = {0};
    char end_text[16] = {0};
    uint32_t configured_league_id = kbo_resolve_kbo_league_id();
    if (!kbo_format_ymd(file_start, start_text, sizeof(start_text))
            || !kbo_format_ymd(file_end, end_text, sizeof(end_text))) {
        snprintf(start_text, sizeof(start_text), "-");
        snprintf(end_text, sizeof(end_text), "-");
    }

    const char* status = window_open ? "OPEN" : "CLOSED";
    snprintf(
        out,
        out_size,
        "Foreign waiver priority window [league=%u] (%s): %s [Window=%s ~ %s]",
        configured_league_id,
        source,
        status,
        start_text,
        end_text);
    return 1;
}
