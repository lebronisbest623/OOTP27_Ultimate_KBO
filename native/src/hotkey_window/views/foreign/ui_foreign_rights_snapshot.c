#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ui_foreign_rights_snapshot.h"

#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/sync/lock.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../foreign/rights/query/foreign_waiver_rights_query.h"
#include "../../../foreign/waiver_core/api/foreign_waiver_core.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../runtime/hotkey_window_runtime_shared.h"
#include "../../support/assets/names/support_names.h"
#include "../../support/assets/names/ui_uniform_numbers.h"
#include "../../ui_html_helpers/position_helpers.h"

typedef struct KboForeignRightsUiSnapshotCache {
    LONG generation;
    LONG rights_generation;
    uintptr_t db_ptr;
    int built;
    KboForeignRightsUiSnapshot snapshot;
} KboForeignRightsUiSnapshotCache;

typedef struct KboForeignRightsUiSnapshotJob {
    uint32_t selected_team_id;
    LONG generation;
} KboForeignRightsUiSnapshotJob;

static KboRwLock g_kbo_foreign_rights_ui_snapshot_lock = KBO_RW_LOCK_INIT;
static volatile LONG g_kbo_foreign_rights_ui_snapshot_generation = 1;
static volatile LONG g_kbo_foreign_rights_ui_snapshot_building = 0;
static KboForeignRightsUiSnapshotCache g_kbo_foreign_rights_ui_snapshot_cache;

static void kbo_foreign_rights_ui_snapshot_fill_meta(
    uint32_t selected_team_id,
    KboForeignRightsUiSnapshot* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->selected_team_id = selected_team_id;
    kbo_current_foreign_waiver_window_dates(&out->window_start, &out->window_end);
    kbo_get_foreign_waiver_current_yyyymmdd(&out->today);
    out->window_open = kbo_is_foreign_waiver_negotiation_window_open();
}

static void kbo_foreign_rights_ui_snapshot_fill_row(
    KboForeignRightsUiSnapshotRow* row,
    uint8_t* player,
    uint32_t player_id,
    uint32_t selected_team_id,
    uint32_t today,
    uint32_t window_end)
{
    if (row == NULL || player == NULL) {
        return;
    }
    memset(row, 0, sizeof(*row));
    row->player_id = player_id;
    row->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    row->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    row->age = memory_range_readable(player + OOTP27_PLAYER_AGE_OFFSET, sizeof(uint16_t))
        ? *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET)
        : 0u;
    row->restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    row->secondary_restricted = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    row->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
    row->loan_active = player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET];
    row->injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];

    kbo_hub_copy_player_display_name(player, row->player_name, sizeof(row->player_name));
    kbo_hub_copy_team_abbrev_by_id(row->current_team_id, row->team_abbrev, sizeof(row->team_abbrev), NULL);
    kbo_webview_copy_player_uniform_number(player_id, row->uniform_number, sizeof(row->uniform_number));
    snprintf(row->position_label, sizeof(row->position_label), "%s", kbo_webview_player_position_label(player, 0u));

    char latest_action[16] = {0};
    int has_decision = kbo_foreign_waiver_latest_decision_action(
        window_end,
        selected_team_id,
        player_id,
        latest_action,
        sizeof(latest_action));
    row->skip_chosen = has_decision && _stricmp(latest_action, "SKIP") == 0;
    row->retain_requested = has_decision && _stricmp(latest_action, "RETAIN") == 0;
    row->has_active_right = today != 0u
        && kbo_has_active_foreign_waiver_right(selected_team_id, player_id, today)
        && !row->skip_chosen;
    if (row->has_active_right) {
        kbo_get_active_foreign_waiver_right_dates(
            selected_team_id,
            player_id,
            today,
            &row->retained_on,
            &row->expires_on);
    }
}

static void kbo_foreign_rights_ui_snapshot_post_refresh(void)
{
    HWND hwnd = g_kbo_hotkey_window;
    if (hwnd != NULL && IsWindow(hwnd) && IsWindowVisible(hwnd)) {
        PostMessageA(hwnd, KBO_WM_SHOW_HUB_CONTENT, 0, 0);
    }
}

static DWORD WINAPI kbo_foreign_rights_ui_snapshot_worker(LPVOID parameter)
{
    KboForeignRightsUiSnapshotJob* job = (KboForeignRightsUiSnapshotJob*)parameter;
    if (job == NULL) {
        InterlockedExchange(&g_kbo_foreign_rights_ui_snapshot_building, 0);
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_foreign_rights_snapshot_build);
    KboForeignRightsUiSnapshot snapshot;
    kbo_foreign_rights_ui_snapshot_fill_meta(job->selected_team_id, &snapshot);
    kbo_ensure_foreign_waiver_rights_loaded_for_lookup();
    if (snapshot.top_player_id == 0u) {
        kbo_resolve_foreign_waiver_top_candidate_for_team(
            job->selected_team_id,
            &snapshot.top_player_id,
            &snapshot.top_current_team_id);
    }

    LONG rights_generation_before =
        InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_generation, 0, 0);
    uintptr_t db_ptr = get_ootp_cached_global_database();
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        for (int32_t i = 0; i < player_count && snapshot.count < KBO_FOREIGN_RIGHTS_UI_MAX_ROWS; i++) {
            uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
            if (!kbo_player_pointer_plausible(player_ptr)) {
                continue;
            }
            uint8_t* player = (uint8_t*)player_ptr;
            uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
            if (player_id == 0u || !kbo_player_is_foreign_for_kbo_rights(player)) {
                continue;
            }
            uint32_t decision_team_id = kbo_get_foreign_waiver_decision_team_id(player);
            if (snapshot.window_open && decision_team_id != job->selected_team_id) {
                continue;
            }

            KboForeignRightsUiSnapshotRow row;
            kbo_foreign_rights_ui_snapshot_fill_row(
                &row,
                player,
                player_id,
                job->selected_team_id,
                snapshot.today,
                snapshot.window_end);
            if (!snapshot.window_open && !row.has_active_right) {
                continue;
            }
            snapshot.rows[snapshot.count++] = row;
        }
    }

    LONG generation = InterlockedCompareExchange(
        &g_kbo_foreign_rights_ui_snapshot_generation,
        0,
        0);
    LONG rights_generation_after =
        InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_generation, 0, 0);
    if (generation == job->generation
            && rights_generation_before == rights_generation_after
            && db_ptr == get_ootp_cached_global_database()) {
        kbo_rw_lock_enter_exclusive(&g_kbo_foreign_rights_ui_snapshot_lock);
        g_kbo_foreign_rights_ui_snapshot_cache.generation = generation;
        g_kbo_foreign_rights_ui_snapshot_cache.rights_generation = rights_generation_after;
        g_kbo_foreign_rights_ui_snapshot_cache.db_ptr = db_ptr;
        g_kbo_foreign_rights_ui_snapshot_cache.snapshot = snapshot;
        g_kbo_foreign_rights_ui_snapshot_cache.built = 1;
        kbo_rw_lock_leave_exclusive(&g_kbo_foreign_rights_ui_snapshot_lock);
    }

    KBO_PROFILE_END(profile_foreign_rights_snapshot_build, "webview.foreign_rights.snapshot_build");
    HeapFree(GetProcessHeap(), 0, job);
    InterlockedExchange(&g_kbo_foreign_rights_ui_snapshot_building, 0);
    kbo_foreign_rights_ui_snapshot_post_refresh();
    return 0;
}

static void kbo_foreign_rights_ui_snapshot_schedule(uint32_t selected_team_id, LONG generation)
{
    if (InterlockedCompareExchange(&g_kbo_foreign_rights_ui_snapshot_building, 1, 0) != 0) {
        return;
    }

    KboForeignRightsUiSnapshotJob* job = (KboForeignRightsUiSnapshotJob*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(*job));
    if (job == NULL) {
        InterlockedExchange(&g_kbo_foreign_rights_ui_snapshot_building, 0);
        return;
    }
    job->selected_team_id = selected_team_id;
    job->generation = generation;
    if (!kbo_start_runtime_thread(kbo_foreign_rights_ui_snapshot_worker, job, "f2 foreign rights snapshot")) {
        HeapFree(GetProcessHeap(), 0, job);
        InterlockedExchange(&g_kbo_foreign_rights_ui_snapshot_building, 0);
    }
}

int kbo_foreign_rights_ui_snapshot_get(
    uint32_t selected_team_id,
    KboForeignRightsUiSnapshot* out_snapshot,
    int* out_updating)
{
    if (out_updating != NULL) {
        *out_updating = 0;
    }
    if (out_snapshot == NULL) {
        return 0;
    }

    kbo_foreign_rights_ui_snapshot_fill_meta(selected_team_id, out_snapshot);
    LONG generation = InterlockedCompareExchange(
        &g_kbo_foreign_rights_ui_snapshot_generation,
        0,
        0);
    LONG rights_generation = InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_generation, 0, 0);
    uintptr_t db_ptr = get_ootp_cached_global_database();

    int ready = 0;
    int stale = 0;
    kbo_rw_lock_enter_shared(&g_kbo_foreign_rights_ui_snapshot_lock);
    const KboForeignRightsUiSnapshotCache* cache = &g_kbo_foreign_rights_ui_snapshot_cache;
    if (cache->built && cache->snapshot.selected_team_id == selected_team_id) {
        int current = cache->generation == generation
            && cache->rights_generation == rights_generation
            && cache->db_ptr == db_ptr
            && cache->snapshot.today == out_snapshot->today
            && cache->snapshot.window_start == out_snapshot->window_start
            && cache->snapshot.window_end == out_snapshot->window_end
            && cache->snapshot.window_open == out_snapshot->window_open;
        if (current || cache->db_ptr == db_ptr) {
            *out_snapshot = cache->snapshot;
            ready = current;
            stale = !current;
        }
    }
    kbo_rw_lock_leave_shared(&g_kbo_foreign_rights_ui_snapshot_lock);

    if (!ready && selected_team_id != 0u) {
        if (out_updating != NULL) {
            *out_updating = 1;
        }
        kbo_foreign_rights_ui_snapshot_schedule(selected_team_id, generation);
    }
    return ready || stale;
}

void kbo_foreign_rights_ui_snapshot_invalidate(void)
{
    InterlockedIncrement(&g_kbo_foreign_rights_ui_snapshot_generation);
}
