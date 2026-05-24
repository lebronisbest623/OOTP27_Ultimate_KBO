#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ui_secondary_draft_list_snapshot.h"

#include <string.h>

#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/sync/lock.h"
#include "../../../custom_events/secondary_draft/secondary_draft_internal.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../runtime/hotkey_window_runtime_shared.h"

typedef struct KboSecondaryDraftListUiSnapshotCache {
    LONG generation;
    uintptr_t db_ptr;
    int built;
    KboSecondaryDraftListUiSnapshot snapshot;
} KboSecondaryDraftListUiSnapshotCache;

typedef struct KboSecondaryDraftListUiSnapshotJob {
    uint32_t season;
    uint32_t team_id;
    uint32_t today;
    LONG generation;
} KboSecondaryDraftListUiSnapshotJob;

static KboRwLock g_kbo_secondary_draft_ui_list_snapshot_lock = KBO_RW_LOCK_INIT;
static volatile LONG g_kbo_secondary_draft_ui_list_snapshot_generation = 1;
static volatile LONG g_kbo_secondary_draft_ui_list_snapshot_building = 0;
static KboSecondaryDraftListUiSnapshotCache g_kbo_secondary_draft_ui_list_snapshot_cache;

static uint32_t kbo_secondary_draft_ui_list_snapshot_today(void)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_tick_latest_components(&year, &month, &day)
            || year == 0u
            || month == 0u
            || day == 0u) {
        return 0u;
    }
    return year * 10000u + month * 100u + day;
}

static void kbo_secondary_draft_ui_list_snapshot_fill_meta(
    uint32_t season,
    uint32_t team_id,
    uint32_t today,
    KboSecondaryDraftListUiSnapshot* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->season = season;
    out->team_id = team_id;
    out->today = today;
    if (season != 0u) {
        out->has_window = kbo_secondary_draft_load_window(season, &out->window);
        out->window_open = kbo_secondary_draft_protection_window_open(season);
    }
    if (season != 0u
            && team_id != 0u
            && kbo_secondary_draft_ensure_schema("secondary_draft_ui_list_snapshot_meta")) {
        out->saved_count = kbo_secondary_draft_sql_protected_count(season, team_id);
        out->submitted = kbo_secondary_draft_sql_team_submitted(season, team_id, NULL);
    }
}

static void kbo_secondary_draft_ui_list_snapshot_post_refresh(void)
{
    HWND hwnd = g_kbo_hotkey_window;
    if (hwnd != NULL && IsWindow(hwnd) && IsWindowVisible(hwnd)) {
        PostMessageA(hwnd, KBO_WM_SHOW_HUB_CONTENT, 0, 0);
    }
}

static DWORD WINAPI kbo_secondary_draft_ui_list_snapshot_worker(LPVOID parameter)
{
    KboSecondaryDraftListUiSnapshotJob* job = (KboSecondaryDraftListUiSnapshotJob*)parameter;
    if (job == NULL) {
        InterlockedExchange(&g_kbo_secondary_draft_ui_list_snapshot_building, 0);
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_secondary_draft_list_snapshot_build);
    KboSecondaryDraftListUiSnapshot snapshot;
    kbo_secondary_draft_ui_list_snapshot_fill_meta(
        job->season,
        job->team_id,
        job->today,
        &snapshot);
    uintptr_t db_ptr = get_ootp_cached_global_database();
    if (job->season != 0u && job->team_id != 0u) {
        int count = kbo_secondary_draft_collect_team_list_rows(
            job->season,
            job->team_id,
            snapshot.rows,
            KBO_SECONDARY_DRAFT_UI_MAX_CANDIDATES,
            &snapshot.submitted,
            &snapshot.saved_count);
        if (count < 0) {
            count = 0;
        }
        if (count > KBO_SECONDARY_DRAFT_UI_MAX_CANDIDATES) {
            count = KBO_SECONDARY_DRAFT_UI_MAX_CANDIDATES;
        }
        snapshot.count = count;
    }

    LONG generation = InterlockedCompareExchange(
        &g_kbo_secondary_draft_ui_list_snapshot_generation,
        0,
        0);
    if (generation == job->generation && db_ptr == get_ootp_cached_global_database()) {
        kbo_rw_lock_enter_exclusive(&g_kbo_secondary_draft_ui_list_snapshot_lock);
        g_kbo_secondary_draft_ui_list_snapshot_cache.generation = generation;
        g_kbo_secondary_draft_ui_list_snapshot_cache.db_ptr = db_ptr;
        g_kbo_secondary_draft_ui_list_snapshot_cache.snapshot = snapshot;
        g_kbo_secondary_draft_ui_list_snapshot_cache.built = 1;
        kbo_rw_lock_leave_exclusive(&g_kbo_secondary_draft_ui_list_snapshot_lock);
    }

    KBO_PROFILE_END(
        profile_secondary_draft_list_snapshot_build,
        "webview.secondary_draft.list.snapshot_build");
    HeapFree(GetProcessHeap(), 0, job);
    InterlockedExchange(&g_kbo_secondary_draft_ui_list_snapshot_building, 0);
    kbo_secondary_draft_ui_list_snapshot_post_refresh();
    return 0;
}

static void kbo_secondary_draft_ui_list_snapshot_schedule(
    uint32_t season,
    uint32_t team_id,
    uint32_t today,
    LONG generation)
{
    if (InterlockedCompareExchange(&g_kbo_secondary_draft_ui_list_snapshot_building, 1, 0) != 0) {
        return;
    }

    KboSecondaryDraftListUiSnapshotJob* job = (KboSecondaryDraftListUiSnapshotJob*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(*job));
    if (job == NULL) {
        InterlockedExchange(&g_kbo_secondary_draft_ui_list_snapshot_building, 0);
        return;
    }
    job->season = season;
    job->team_id = team_id;
    job->today = today;
    job->generation = generation;
    if (!kbo_start_runtime_thread(
            kbo_secondary_draft_ui_list_snapshot_worker,
            job,
            "f2 secondary draft list snapshot")) {
        HeapFree(GetProcessHeap(), 0, job);
        InterlockedExchange(&g_kbo_secondary_draft_ui_list_snapshot_building, 0);
    }
}

int kbo_secondary_draft_ui_list_snapshot_get(
    uint32_t season,
    uint32_t team_id,
    KboSecondaryDraftListUiSnapshot* out_snapshot,
    int* out_updating)
{
    if (out_updating != NULL) {
        *out_updating = 0;
    }
    if (out_snapshot == NULL) {
        return 0;
    }

    uint32_t today = kbo_secondary_draft_ui_list_snapshot_today();
    kbo_secondary_draft_ui_list_snapshot_fill_meta(season, team_id, today, out_snapshot);
    LONG generation = InterlockedCompareExchange(
        &g_kbo_secondary_draft_ui_list_snapshot_generation,
        0,
        0);
    uintptr_t db_ptr = get_ootp_cached_global_database();

    int ready = 0;
    int stale = 0;
    kbo_rw_lock_enter_shared(&g_kbo_secondary_draft_ui_list_snapshot_lock);
    const KboSecondaryDraftListUiSnapshotCache* cache =
        &g_kbo_secondary_draft_ui_list_snapshot_cache;
    if (cache->built
            && cache->snapshot.season == season
            && cache->snapshot.team_id == team_id) {
        int current = cache->generation == generation
            && cache->db_ptr == db_ptr
            && cache->snapshot.today == today;
        if (current || cache->db_ptr == db_ptr) {
            *out_snapshot = cache->snapshot;
            ready = current;
            stale = !current;
        }
    }
    kbo_rw_lock_leave_shared(&g_kbo_secondary_draft_ui_list_snapshot_lock);

    if (!ready && season != 0u && team_id != 0u) {
        if (out_updating != NULL) {
            *out_updating = 1;
        }
        kbo_secondary_draft_ui_list_snapshot_schedule(season, team_id, today, generation);
    }
    return ready || stale;
}

void kbo_secondary_draft_ui_list_snapshot_invalidate(void)
{
    InterlockedIncrement(&g_kbo_secondary_draft_ui_list_snapshot_generation);
}
