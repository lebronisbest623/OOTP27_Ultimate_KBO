#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../independent_acquisition_ai_module.h"
#include "../ai/lifecycle/independent_acquisition_ai_lifecycle.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../core/dates/core_text_date.h"
#include "../../../core/dates/constants/kbo_date_constants.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/policy/foreign_player_policy.h"
#include "sql/independent_acquisition_ai_cursor_sql_store.h"
#include "../window/independent_acquisition_window.h"

volatile LONG g_kbo_independent_acquisition_ai_last_processed_date = 0;
char g_kbo_independent_acquisition_ai_cursor_save_path[MAX_PATH] = {0};

int kbo_independent_acquisition_window_active_silent(uint32_t today)
{
    KBO_PROFILE_BEGIN(profile_independent_acquisition_window_active_silent);
    int active = kbo_independent_team_acquisition_window_active(today, NULL, NULL);
    KBO_PROFILE_END(
        profile_independent_acquisition_window_active_silent,
        active
            ? "independent_acquisition.window_active_silent.active"
            : "independent_acquisition.window_active_silent.closed");
    return active;
}

uint32_t kbo_independent_acquisition_next_date(uint32_t today)
{
    return kbo_add_days_yyyymmdd(today, 1u);
}

static int kbo_independent_acquisition_ai_cursor_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_independent_acquisition_load_processed_date(void)
{
    KBO_PROFILE_BEGIN(profile_independent_acquisition_cursor_load);
    uint32_t value = 0u;
    if (!kbo_independent_acquisition_ai_cursor_sql_load(&value)) {
        KBO_PROFILE_END(
            profile_independent_acquisition_cursor_load,
            "independent_acquisition.cursor.load.failed");
        return 0u;
    }
    if (value < KBO_SEASON_DATE_MIN || value > KBO_SIM_DATE_MAX) {
        KBO_PROFILE_END(
            profile_independent_acquisition_cursor_load,
            "independent_acquisition.cursor.load.invalid");
        return 0u;
    }
    KBO_PROFILE_END(
        profile_independent_acquisition_cursor_load,
        "independent_acquisition.cursor.load.ok");
    return value;
}

static void kbo_independent_acquisition_persist_processed_date(uint32_t today, const char* source)
{
    KBO_PROFILE_BEGIN(profile_independent_acquisition_cursor_persist);
    char path[MAX_PATH] = {0};
    if (today == 0u || !kbo_independent_acquisition_ai_cursor_path(path, sizeof(path))) {
        KBO_PROFILE_END(
            profile_independent_acquisition_cursor_persist,
            "independent_acquisition.cursor.persist.no_path");
        return;
    }

    if (!kbo_independent_acquisition_ai_cursor_sql_store(today, source)) {
        kbo_log_runtimef(
            "independent acquisition AI cursor persist failed source=%s date=%u reason=sqlite_write_failed path=%s",
            source != NULL ? source : "",
            today,
            path);
        KBO_PROFILE_END(
            profile_independent_acquisition_cursor_persist,
            "independent_acquisition.cursor.persist.failed");
        return;
    }
    KBO_PROFILE_END(
        profile_independent_acquisition_cursor_persist,
        "independent_acquisition.cursor.persist.ok");
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
    KBO_PROFILE_BEGIN(profile_independent_acquisition_cursor_scope);
    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        KBO_PROFILE_END(
            profile_independent_acquisition_cursor_scope,
            "independent_acquisition.cursor_scope.no_save");
        return;
    }
    if (g_kbo_independent_acquisition_ai_cursor_save_path[0] != '\0'
            && strcmp(g_kbo_independent_acquisition_ai_cursor_save_path, save_path) == 0) {
        KBO_PROFILE_END(
            profile_independent_acquisition_cursor_scope,
            "independent_acquisition.cursor_scope.unchanged");
        return;
    }

    snprintf(
        g_kbo_independent_acquisition_ai_cursor_save_path,
        sizeof(g_kbo_independent_acquisition_ai_cursor_save_path),
        "%s",
        save_path);
    InterlockedExchange(&g_kbo_independent_acquisition_ai_last_processed_date, 0);
    KBO_PROFILE_END(
        profile_independent_acquisition_cursor_scope,
        "independent_acquisition.cursor_scope.changed");
}

uint32_t kbo_independent_acquisition_processed_date(void)
{
    KBO_PROFILE_BEGIN(profile_independent_acquisition_processed_date);
    kbo_independent_acquisition_sync_cursor_save_scope();

    uint32_t cached = kbo_independent_acquisition_observed_processed_date();
    if (cached != 0u) {
        KBO_PROFILE_END(
            profile_independent_acquisition_processed_date,
            "independent_acquisition.processed_date.cached");
        return cached;
    }

    uint32_t loaded = kbo_independent_acquisition_load_processed_date();
    if (loaded != 0u) {
        InterlockedCompareExchange(
            &g_kbo_independent_acquisition_ai_last_processed_date,
            (LONG)loaded,
            0);
    }
    KBO_PROFILE_END(
        profile_independent_acquisition_processed_date,
        loaded != 0u
            ? "independent_acquisition.processed_date.loaded"
            : "independent_acquisition.processed_date.miss");
    return loaded;
}

void kbo_independent_acquisition_mark_processed_date(uint32_t today, const char* source)
{
    KBO_PROFILE_BEGIN(profile_independent_acquisition_mark_processed_date);
    if (today != 0u) {
        kbo_independent_acquisition_sync_cursor_save_scope();
        InterlockedExchange(
            &g_kbo_independent_acquisition_ai_last_processed_date,
            (LONG)today);
        kbo_independent_acquisition_persist_processed_date(today, source);
        KBO_PROFILE_END(
            profile_independent_acquisition_mark_processed_date,
            "independent_acquisition.mark_processed_date.ok");
        return;
    }
    KBO_PROFILE_END(
        profile_independent_acquisition_mark_processed_date,
        "independent_acquisition.mark_processed_date.zero");
}

KboIndependentAcquisitionSellerAvailability
kbo_independent_acquisition_collect_available_sellers_for_date(
    uint32_t today,
    KboIndependentFuturesTeamLeague* out_available_sellers,
    int max_available_sellers)
{
    KBO_PROFILE_BEGIN(profile_independent_acquisition_seller_availability_total);
    KboIndependentAcquisitionSellerAvailability availability;
    memset(&availability, 0, sizeof(availability));
    availability.seller_transfer_limit =
        kbo_foreign_player_policy()->independent_acquisition_seller_transfer_limit;

    KboIndependentFuturesTeamLeague sellers[KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS];
    memset(sellers, 0, sizeof(sellers));
    KBO_PROFILE_BEGIN(profile_independent_acquisition_seller_seed_collect);
    availability.seller_count = kbo_collect_independent_futures_team_leagues(
        sellers,
        KBO_INDEPENDENT_ACQUISITION_MAX_SELLERS,
        &availability.seed_rows,
        &availability.unresolved_rows);
    KBO_PROFILE_END(
        profile_independent_acquisition_seller_seed_collect,
        "independent_acquisition.seller_availability.seed_collect");
    if (availability.seller_count <= 0) {
        KBO_PROFILE_END(
            profile_independent_acquisition_seller_availability_total,
            "independent_acquisition.seller_availability.no_sellers");
        return availability;
    }

    uint32_t season = kbo_independent_acquisition_effective_season(today);
    for (int i = 0; i < availability.seller_count; i++) {
        KBO_PROFILE_BEGIN(profile_independent_acquisition_seller_transfer_count);
        int transfers = kbo_independent_acquisition_transferred_count(season, sellers[i].team_id);
        KBO_PROFILE_END(
            profile_independent_acquisition_seller_transfer_count,
            "independent_acquisition.seller_availability.transferred_count");
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

    KBO_PROFILE_END(
        profile_independent_acquisition_seller_availability_total,
        availability.available_seller_count > 0
            ? "independent_acquisition.seller_availability.available"
            : "independent_acquisition.seller_availability.capped");
    return availability;
}

int kbo_independent_acquisition_has_pending_requests(uint32_t today)
{
    KBO_PROFILE_BEGIN(profile_independent_acquisition_pending_requests);
    uint32_t season = kbo_independent_acquisition_effective_season(today);
    KboIndependentAcquisitionQueuedRequest pending;
    memset(&pending, 0, sizeof(pending));
    int has_pending = kbo_independent_acquisition_load_requests(season, &pending, 1) > 0;
    KBO_PROFILE_END(
        profile_independent_acquisition_pending_requests,
        has_pending
            ? "independent_acquisition.pending_requests.hit"
            : "independent_acquisition.pending_requests.miss");
    return has_pending;
}
