#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_module.h"
#include "../ai/lifecycle/independent_acquisition_ai_lifecycle.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../core/dates/core_text_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/policy/foreign_player_policy.h"
#include "../window/independent_acquisition_window.h"

volatile LONG g_kbo_independent_acquisition_ai_last_processed_date = 0;
char g_kbo_independent_acquisition_ai_cursor_save_path[MAX_PATH] = {0};

int kbo_independent_acquisition_window_active_silent(uint32_t today)
{
    return kbo_independent_team_acquisition_window_active(today, NULL, NULL);
}

uint32_t kbo_independent_acquisition_next_date(uint32_t today)
{
    return kbo_add_days_yyyymmdd(today, 1u);
}

static int kbo_independent_acquisition_ai_cursor_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_INDEPENDENT_ACQUISITION_AI_CURSOR_FILE,
        out,
        out_size);
}

static uint32_t kbo_independent_acquisition_load_processed_date(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_independent_acquisition_ai_cursor_path(path, sizeof(path))) {
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
    if (value < 19820101ul || value > 22001231ul) {
        return 0u;
    }
    return (uint32_t)value;
}

static void kbo_independent_acquisition_persist_processed_date(uint32_t today, const char* source)
{
    char path[MAX_PATH] = {0};
    if (today == 0u || !kbo_independent_acquisition_ai_cursor_path(path, sizeof(path))) {
        return;
    }

    char text[32] = {0};
    snprintf(text, sizeof(text), "%u\r\n", today);
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
            "independent acquisition AI cursor persist skipped source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            today,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    DWORD written = 0u;
    DWORD len = (DWORD)strlen(text);
    if (!WriteFile(file, text, len, &written, NULL) || written != len) {
        kbo_log_runtimef(
            "independent acquisition AI cursor persist failed source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            today,
            (unsigned long)GetLastError(),
            path);
    }
    CloseHandle(file);
}

static uint32_t kbo_independent_acquisition_observed_processed_date(void)
{
    return (uint32_t)InterlockedCompareExchange(
        &g_kbo_independent_acquisition_ai_last_processed_date,
        0,
        0);
}

static void kbo_independent_acquisition_sync_cursor_save_scope(void)
{
    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return;
    }
    if (g_kbo_independent_acquisition_ai_cursor_save_path[0] != '\0'
            && strcmp(g_kbo_independent_acquisition_ai_cursor_save_path, save_path) == 0) {
        return;
    }

    snprintf(
        g_kbo_independent_acquisition_ai_cursor_save_path,
        sizeof(g_kbo_independent_acquisition_ai_cursor_save_path),
        "%s",
        save_path);
    InterlockedExchange(&g_kbo_independent_acquisition_ai_last_processed_date, 0);
}

uint32_t kbo_independent_acquisition_processed_date(void)
{
    kbo_independent_acquisition_sync_cursor_save_scope();

    uint32_t cached = kbo_independent_acquisition_observed_processed_date();
    if (cached != 0u) {
        return cached;
    }

    uint32_t loaded = kbo_independent_acquisition_load_processed_date();
    if (loaded != 0u) {
        InterlockedCompareExchange(
            &g_kbo_independent_acquisition_ai_last_processed_date,
            (LONG)loaded,
            0);
    }
    return loaded;
}

void kbo_independent_acquisition_mark_processed_date(uint32_t today, const char* source)
{
    if (today != 0u) {
        kbo_independent_acquisition_sync_cursor_save_scope();
        InterlockedExchange(
            &g_kbo_independent_acquisition_ai_last_processed_date,
            (LONG)today);
        kbo_independent_acquisition_persist_processed_date(today, source);
    }
}

KboIndependentAcquisitionSellerAvailability
kbo_independent_acquisition_collect_available_sellers_for_date(
    uint32_t today,
    KboIndependentFuturesTeamLeague* out_available_sellers,
    int max_available_sellers)
{
    KboIndependentAcquisitionSellerAvailability availability;
    memset(&availability, 0, sizeof(availability));
    availability.seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    memset(sellers, 0, sizeof(sellers));
    availability.seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        &availability.seed_rows,
        &availability.unresolved_rows);
    if (availability.seller_count <= 0) {
        return availability;
    }

    uint32_t season = kbo_independent_acquisition_effective_season(today);
    for (int i = 0; i < availability.seller_count; i++) {
        int transfers = kbo_independent_acquisition_transferred_count(season, sellers[i].team_id);
        if (transfers >= availability.seller_transfer_limit) {
            availability.capped_sellers++;
            continue;
        }
        if (out_available_sellers != NULL
                && availability.available_seller_count < max_available_sellers) {
            out_available_sellers[availability.available_seller_count] = sellers[i];
        }
        availability.available_seller_count++;
    }

    return availability;
}

int kbo_independent_acquisition_has_pending_requests(uint32_t today)
{
    uint32_t season = kbo_independent_acquisition_effective_season(today);
    KboIndependentAcquisitionQueuedRequest pending;
    memset(&pending, 0, sizeof(pending));
    return kbo_independent_acquisition_load_requests(season, &pending, 1) > 0;
}
