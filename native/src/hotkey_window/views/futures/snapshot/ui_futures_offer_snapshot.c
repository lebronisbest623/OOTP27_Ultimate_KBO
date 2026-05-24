#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ui_futures_offer_snapshot.h"

#include <stdio.h>
#include <string.h>

#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/sync/lock.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../runtime/hotkey_window_runtime_shared.h"
#include "../../../support/assets/nations/ui_nation_helpers.h"
#include "../ui_futures_league_view_helpers.h"

typedef struct KboFuturesOfferUiSnapshotCache {
    LONG snapshot_generation;
    LONG offer_generation;
    uintptr_t db_ptr;
    int built;
    KboFuturesOfferUiSnapshot snapshot;
} KboFuturesOfferUiSnapshotCache;

typedef struct KboFuturesOfferUiSnapshotJob {
    uint32_t buyer_team_id;
    LONG snapshot_generation;
} KboFuturesOfferUiSnapshotJob;

static KboRwLock g_kbo_futures_offer_ui_snapshot_lock = KBO_RW_LOCK_INIT;
static volatile LONG g_kbo_futures_offer_ui_snapshot_generation = 1;
static volatile LONG g_kbo_futures_offer_ui_snapshot_building = 0;
static KboFuturesOfferUiSnapshotCache g_kbo_futures_offer_ui_snapshot_cache;

static int kbo_futures_offer_ui_snapshot_context_matches(
    const KboIndependentAcquisitionUiContext* left,
    const KboIndependentAcquisitionUiContext* right)
{
    return left != NULL
        && right != NULL
        && left->today == right->today
        && left->season == right->season
        && left->buyer_team_id == right->buyer_team_id
        && left->open_date == right->open_date
        && left->close_date == right->close_date
        && left->window_open == right->window_open
        && left->policy_enabled == right->policy_enabled
        && left->buyer_valid == right->buyer_valid
        && left->seller_count == right->seller_count
        && left->seed_rows == right->seed_rows
        && left->unresolved_seed_rows == right->unresolved_seed_rows
        && left->buyer_active_count == right->buyer_active_count
        && left->buyer_effective_foreign_count == right->buyer_effective_foreign_count
        && left->buyer_cash == right->buyer_cash;
}

static void kbo_futures_offer_ui_snapshot_copy_text(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    snprintf(out, out_size, "%s", text != NULL ? text : "");
}

static void kbo_futures_offer_ui_snapshot_copy_nation(
    uint32_t nation_id,
    char* out_label,
    size_t label_size,
    char* out_abbrev,
    size_t abbrev_size)
{
    kbo_futures_offer_ui_snapshot_copy_text(
        out_label,
        label_size,
        kbo_hub_nation_label_for_id(nation_id));
    kbo_futures_offer_ui_snapshot_copy_text(
        out_abbrev,
        abbrev_size,
        kbo_hub_nation_abbrev_for_id(nation_id));
}

static void kbo_futures_offer_ui_snapshot_fill_row(
    const KboIndependentAcquisitionUiOfferRow* source,
    KboFuturesOfferUiSnapshotRow* dest)
{
    if (source == NULL || dest == NULL) {
        return;
    }
    memset(dest, 0, sizeof(*dest));
    dest->player_id = source->player_id;
    dest->seller_team_id = source->seller_team_id;
    dest->nation_id = source->nation_id;
    dest->age = source->age;
    dest->offer_blocked = source->offer_blocked;
    dest->already_requested = source->already_requested;
    dest->already_decided = source->already_decided;
    dest->cash_cost = source->cash_cost;
    kbo_futures_ui_copy_player_name(
        source->player_ptr,
        source->player_id,
        dest->player_name,
        sizeof(dest->player_name));
    kbo_futures_ui_copy_team_name(
        source->seller_team_id,
        dest->seller_name,
        sizeof(dest->seller_name));
    kbo_futures_offer_ui_snapshot_copy_text(
        dest->position_label,
        sizeof(dest->position_label),
        kbo_futures_ui_position_label(source->player_ptr));
    kbo_futures_offer_ui_snapshot_copy_nation(
        source->nation_id,
        dest->nation_label,
        sizeof(dest->nation_label),
        dest->nation_abbrev,
        sizeof(dest->nation_abbrev));
    kbo_futures_offer_ui_snapshot_copy_text(
        dest->slot_label,
        sizeof(dest->slot_label),
        source->slot_label);
    kbo_futures_offer_ui_snapshot_copy_text(
        dest->status_label,
        sizeof(dest->status_label),
        source->status_label);
    kbo_futures_ui_format_cash(source->cash_cost, dest->cash_text, sizeof(dest->cash_text));
}

static void kbo_futures_offer_ui_snapshot_post_refresh(void)
{
    HWND hwnd = g_kbo_hotkey_window;
    if (hwnd != NULL && IsWindow(hwnd) && IsWindowVisible(hwnd)) {
        PostMessageA(hwnd, KBO_WM_SHOW_HUB_CONTENT, 0, 0);
    }
}

static DWORD WINAPI kbo_futures_offer_ui_snapshot_worker(LPVOID parameter)
{
    KboFuturesOfferUiSnapshotJob* job = (KboFuturesOfferUiSnapshotJob*)parameter;
    if (job == NULL) {
        InterlockedExchange(&g_kbo_futures_offer_ui_snapshot_building, 0);
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_futures_offer_snapshot_build);
    KboFuturesOfferUiSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.buyer_team_id = job->buyer_team_id;
    LONG offer_generation_before = kbo_independent_acquisition_ui_offer_cache_generation();
    uintptr_t db_ptr = get_ootp_cached_global_database();

    KboIndependentAcquisitionUiOfferRow rows[KBO_INDEPENDENT_ACQUISITION_UI_MAX_OFFERS];
    memset(rows, 0, sizeof(rows));
    int count = kbo_independent_acquisition_ui_collect_offer_rows(
        job->buyer_team_id,
        rows,
        KBO_INDEPENDENT_ACQUISITION_UI_MAX_OFFERS,
        &snapshot.context);
    if (count < 0) {
        count = 0;
    }
    if (count > KBO_INDEPENDENT_ACQUISITION_UI_MAX_OFFERS) {
        count = KBO_INDEPENDENT_ACQUISITION_UI_MAX_OFFERS;
    }
    snapshot.count = count;
    for (int i = 0; i < count; i++) {
        kbo_futures_offer_ui_snapshot_fill_row(&rows[i], &snapshot.rows[i]);
    }

    LONG snapshot_generation = InterlockedCompareExchange(
        &g_kbo_futures_offer_ui_snapshot_generation,
        0,
        0);
    LONG offer_generation_after = kbo_independent_acquisition_ui_offer_cache_generation();
    if (snapshot_generation == job->snapshot_generation
            && offer_generation_before == offer_generation_after
            && db_ptr == get_ootp_cached_global_database()) {
        kbo_rw_lock_enter_exclusive(&g_kbo_futures_offer_ui_snapshot_lock);
        g_kbo_futures_offer_ui_snapshot_cache.snapshot_generation = snapshot_generation;
        g_kbo_futures_offer_ui_snapshot_cache.offer_generation = offer_generation_after;
        g_kbo_futures_offer_ui_snapshot_cache.db_ptr = db_ptr;
        g_kbo_futures_offer_ui_snapshot_cache.snapshot = snapshot;
        g_kbo_futures_offer_ui_snapshot_cache.built = 1;
        kbo_rw_lock_leave_exclusive(&g_kbo_futures_offer_ui_snapshot_lock);
    }

    KBO_PROFILE_END(profile_futures_offer_snapshot_build, "webview.futures.offer.snapshot_build");
    HeapFree(GetProcessHeap(), 0, job);
    InterlockedExchange(&g_kbo_futures_offer_ui_snapshot_building, 0);
    kbo_futures_offer_ui_snapshot_post_refresh();
    return 0;
}

static void kbo_futures_offer_ui_snapshot_schedule(uint32_t buyer_team_id, LONG snapshot_generation)
{
    if (InterlockedCompareExchange(&g_kbo_futures_offer_ui_snapshot_building, 1, 0) != 0) {
        return;
    }

    KboFuturesOfferUiSnapshotJob* job = (KboFuturesOfferUiSnapshotJob*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(*job));
    if (job == NULL) {
        InterlockedExchange(&g_kbo_futures_offer_ui_snapshot_building, 0);
        return;
    }
    job->buyer_team_id = buyer_team_id;
    job->snapshot_generation = snapshot_generation;
    if (!kbo_start_runtime_thread(
            kbo_futures_offer_ui_snapshot_worker,
            job,
            "f2 futures offer snapshot")) {
        HeapFree(GetProcessHeap(), 0, job);
        InterlockedExchange(&g_kbo_futures_offer_ui_snapshot_building, 0);
    }
}

int kbo_futures_offer_ui_snapshot_get(
    uint32_t buyer_team_id,
    KboFuturesOfferUiSnapshot* out_snapshot,
    int* out_updating)
{
    if (out_updating != NULL) {
        *out_updating = 0;
    }
    if (out_snapshot == NULL) {
        return 0;
    }
    memset(out_snapshot, 0, sizeof(*out_snapshot));
    out_snapshot->buyer_team_id = buyer_team_id;
    (void)kbo_independent_acquisition_ui_context(buyer_team_id, &out_snapshot->context);

    LONG snapshot_generation = InterlockedCompareExchange(
        &g_kbo_futures_offer_ui_snapshot_generation,
        0,
        0);
    LONG offer_generation = kbo_independent_acquisition_ui_offer_cache_generation();
    uintptr_t db_ptr = get_ootp_cached_global_database();

    int ready = 0;
    int stale = 0;
    kbo_rw_lock_enter_shared(&g_kbo_futures_offer_ui_snapshot_lock);
    const KboFuturesOfferUiSnapshotCache* cache = &g_kbo_futures_offer_ui_snapshot_cache;
    if (cache->built && cache->snapshot.buyer_team_id == buyer_team_id) {
        int current = cache->snapshot_generation == snapshot_generation
            && cache->offer_generation == offer_generation
            && cache->db_ptr == db_ptr
            && kbo_futures_offer_ui_snapshot_context_matches(
                &cache->snapshot.context,
                &out_snapshot->context);
        if (current || cache->db_ptr == db_ptr) {
            *out_snapshot = cache->snapshot;
            ready = current;
            stale = !current;
        }
    }
    kbo_rw_lock_leave_shared(&g_kbo_futures_offer_ui_snapshot_lock);

    if (!ready) {
        if (buyer_team_id != 0u) {
            if (out_updating != NULL) {
                *out_updating = 1;
            }
            kbo_futures_offer_ui_snapshot_schedule(buyer_team_id, snapshot_generation);
        }
    }
    return ready || stale;
}

void kbo_futures_offer_ui_snapshot_invalidate(void)
{
    InterlockedIncrement(&g_kbo_futures_offer_ui_snapshot_generation);
}
