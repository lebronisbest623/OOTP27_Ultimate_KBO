#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_window.h"
#include "independent_acquisition_open_news.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../custom_events/runtime/dates/custom_event_dates.h"
#include "../../../custom_events/runtime/ledger/custom_event_ledger.h"
#include "../../../core/dates/constants/kbo_date_constants.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/season/phase/season_phase.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"

static volatile LONG g_kbo_independent_team_acquisition_open_date = 0;

#define KBO_INDEPENDENT_TEAM_ACQUISITION_WINDOW_FILE "independent_acquisition_window.txt"
#define KBO_INDEPENDENT_ACQUISITION_REGULAR_SEASON_PLANNING_DAYS 160u

static int kbo_independent_team_acquisition_window_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_INDEPENDENT_TEAM_ACQUISITION_WINDOW_FILE,
        out,
        out_size);
}

static void kbo_independent_team_acquisition_persist_open_date(
    uint32_t event_yyyymmdd,
    const char* source)
{
    char path[MAX_PATH] = {0};
    if (event_yyyymmdd == 0u
            || !kbo_independent_team_acquisition_window_path(path, sizeof(path))) {
        return;
    }

    char text[32] = {0};
    snprintf(text, sizeof(text), "%u\r\n", event_yyyymmdd);
    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "KBO independent futures acquisition window persist skipped source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            event_yyyymmdd,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    DWORD written = 0u;
    DWORD len = (DWORD)strlen(text);
    if (!WriteFile(file, text, len, &written, NULL) || written != len) {
        kbo_log_runtimef(
            "KBO independent futures acquisition window persist failed source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            event_yyyymmdd,
            (unsigned long)GetLastError(),
            path);
    }
    CloseHandle(file);
}

static uint32_t kbo_independent_team_acquisition_load_open_date(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_independent_team_acquisition_window_path(path, sizeof(path))) {
        return 0u;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0u;
    }

    char text[32] = {0};
    DWORD read = 0u;
    int ok = ReadFile(file, text, sizeof(text) - 1u, &read, NULL) && read > 0u;
    CloseHandle(file);
    if (!ok) {
        return 0u;
    }

    unsigned long value = strtoul(text, NULL, 10);
    if (value < KBO_SEASON_DATE_MIN || value > KBO_SIM_DATE_MAX) {
        return 0u;
    }
    return (uint32_t)value;
}

int kbo_handle_independent_team_acquisition_open_event(
    uint32_t event_yyyymmdd,
    const char* source)
{
    if (event_yyyymmdd == 0u) {
        return 0;
    }

    uint32_t existing = kbo_independent_team_acquisition_window_open_date();
    if (existing != 0u
            && existing / 10000u == event_yyyymmdd / 10000u
            && existing != event_yyyymmdd) {
        kbo_log_runtimef(
            "KBO independent futures acquisition duplicate open ignored source=%s event_date=%u existing=%u",
            source != NULL ? source : "",
            event_yyyymmdd,
            existing);
        return 1;
    }

    LONG previous = InterlockedExchange(
        &g_kbo_independent_team_acquisition_open_date,
        (LONG)event_yyyymmdd);
    kbo_independent_team_acquisition_persist_open_date(event_yyyymmdd, source);
    uint32_t news_yyyymmdd = kbo_custom_event_effective_news_date(event_yyyymmdd);
    kbo_log_runtimef(
        "KBO independent futures acquisition window opened source=%s event_date=%u news_date=%u previous=%u",
        source != NULL ? source : "",
        event_yyyymmdd,
        news_yyyymmdd,
        (uint32_t)previous);
    return kbo_emit_independent_team_acquisition_open_news(news_yyyymmdd, source) ? 1 : 0;
}

int kbo_independent_team_acquisition_completion_valid(
    uint32_t league_id,
    uint32_t event_yyyymmdd)
{
    if (league_id == 0u || event_yyyymmdd == 0u) {
        return 0;
    }
    if (kbo_independent_team_acquisition_open_news_completed(event_yyyymmdd, league_id)) {
        return 1;
    }
    return kbo_custom_event_ledger_completed(
        league_id,
        event_yyyymmdd,
        KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN);
}

uint32_t kbo_independent_team_acquisition_window_open_date(void)
{
    uint32_t cached = (uint32_t)InterlockedCompareExchange(
        &g_kbo_independent_team_acquisition_open_date,
        0,
        0);
    if (cached != 0u) {
        return cached;
    }

    uint32_t loaded = kbo_independent_team_acquisition_load_open_date();
    if (loaded != 0u) {
        InterlockedCompareExchange(
            &g_kbo_independent_team_acquisition_open_date,
            (LONG)loaded,
            0);
    }
    return loaded;
}

uint32_t kbo_independent_team_acquisition_window_open_date_for_date(uint32_t today)
{
    uint32_t open_date = kbo_independent_team_acquisition_window_open_date();
    if (today == 0u || open_date == 0u || open_date / 10000u != today / 10000u) {
        return 0u;
    }
    return open_date;
}

uint32_t kbo_independent_team_acquisition_window_planning_days(void)
{
    return KBO_INDEPENDENT_ACQUISITION_REGULAR_SEASON_PLANNING_DAYS;
}

uint32_t kbo_independent_team_acquisition_window_elapsed_days(uint32_t today)
{
    uint32_t open_date = kbo_independent_team_acquisition_window_open_date_for_date(today);
    if (today == 0u || open_date == 0u || today < open_date) {
        return 0u;
    }

    uint32_t open_serial = kbo_date_serial(
        open_date / 10000u,
        (open_date / 100u) % 100u,
        open_date % 100u);
    uint32_t today_serial = kbo_date_serial(
        today / 10000u,
        (today / 100u) % 100u,
        today % 100u);
    if (open_serial == 0u || today_serial == 0u || today_serial < open_serial) {
        return 0u;
    }
    return today_serial - open_serial;
}

int kbo_independent_team_acquisition_window_active(
    uint32_t today,
    uint32_t* out_open_date,
    uint8_t* out_effective_phase)
{
    if (out_open_date != NULL) {
        *out_open_date = 0u;
    }
    if (out_effective_phase != NULL) {
        *out_effective_phase = KBO_SEASON_PHASE_UNKNOWN;
    }

    uint32_t open_date = kbo_independent_team_acquisition_window_open_date_for_date(today);
    if (out_open_date != NULL) {
        *out_open_date = open_date;
    }
    if (today == 0u || open_date == 0u || today < open_date) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }

    KboSeasonPhaseInfo phase_info;
    if (!kbo_season_phase_resolve(league_id, today, 0u, &phase_info)) {
        return 0;
    }
    if (out_effective_phase != NULL) {
        *out_effective_phase = phase_info.effective_phase;
    }
    return phase_info.effective_phase == KBO_SEASON_PHASE_REGULAR_SEASON;
}
