#include "..\amateur_assignment_ortools.h"
#include "../../../../core/runtime_tuning/runtime_tuning_policy.h"

static DWORD WINAPI kbo_amateur_league_batch_flush_thread(LPVOID parameter)
{
    (void)parameter;
    for (;;) {
        if (!kbo_runtime_sleep_should_continue(kbo_runtime_tuning_policy()->amateur_assignment_ortools_batch_sleep_ms)) {
            break;
        }
        if (kbo_amateur_reroute_disabled_cached()) {
            break;
        }
        if (kbo_amateur_flush_league_batch_ortools("idle_post_original", 0)) {
            break;
        }
        kbo_amateur_batch_lock();
        int empty = g_kbo_amateur_league_batch_player_count <= 0;
        kbo_amateur_batch_unlock();
        if (empty) {
            break;
        }
    }
    InterlockedExchange(&g_kbo_amateur_league_batch_flush_thread_started, 0);
    return 0;
}

void kbo_amateur_start_league_batch_flush_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_amateur_league_batch_flush_thread_started, 1, 0) != 0) {
        return;
    }
    HANDLE thread = CreateThread(NULL, 0, kbo_amateur_league_batch_flush_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_kbo_amateur_league_batch_flush_thread_started, 0);
        kbo_log_runtimef("amateur OR-Tools post-add batch flush thread failed gle=%lu", GetLastError());
    }
}

int kbo_amateur_defer_team_add_if_generation(
    uint32_t caller_rva,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8)
{
    (void)team_ptr;
    (void)player_ptr;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    (void)arg6;
    (void)arg7;
    (void)arg8;

    if (!kbo_amateur_generation_team_add_caller(caller_rva)) {
        return 0;
    }

    static volatile LONG inline_log_count = 0;
    LONG slot = InterlockedIncrement(&inline_log_count);
    if (slot <= 5 || (slot % 500) == 0) {
        kbo_log_runtimef(
            "amateur deferred team-add skipped caller_rva=0x%x reason=post_original_batch_reassignment",
            caller_rva);
    } else if (slot == 6) {
        kbo_log_runtime_line(
            "amateur deferred team-add skip log suppressed after 5 calls; logging every 500th call");
    }
    return 0;
}

void kbo_prepare_amateur_assignment_batch_ortools(uintptr_t player_list_ptr, int32_t player_count, uintptr_t source_team_ptr)
{
    (void)source_team_ptr;
    if (player_list_ptr == 0 || player_count <= 1 || player_count > 512) {
        return;
    }
    if (!memory_range_readable((void*)player_list_ptr, (SIZE_T)player_count * sizeof(uintptr_t))) {
        return;
    }

    uintptr_t* players = (uintptr_t*)player_list_ptr;
    uint32_t league_id = 0u;
    uint32_t source_team_id = 0u;
    for (int32_t i = 0; i < player_count; i++) {
        uint8_t* player = (uint8_t*)players[i];
        if (player == NULL) {
            continue;
        }
        uint32_t candidate_league_id = kbo_amateur_player_assignment_league_id(player);
        if (candidate_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID || candidate_league_id == KBO_COLLEGE_LEAGUE_ID) {
            league_id = candidate_league_id;
            source_team_id = kbo_amateur_player_assignment_team_id(player);
            break;
        }
    }
    if (league_id != KBO_HIGH_SCHOOL_LEAGUE_ID && league_id != KBO_COLLEGE_LEAGUE_ID) {
        return;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 1 || candidates == NULL) {
        return;
    }

    kbo_amateur_batch_lock();
    if (g_kbo_amateur_league_batch_league_id != 0u
            && g_kbo_amateur_league_batch_league_id != league_id
            && (g_kbo_amateur_league_batch_player_count > 0
                || g_kbo_amateur_deferred_team_add_count > 0)) {
        if (!kbo_amateur_start_league_switch_flush_locked("league_switch", league_id)) {
            kbo_amateur_batch_unlock();
            kbo_amateur_flush_league_batch_ortools("league_switch", 1);
            kbo_amateur_batch_lock();
        }
    }
    g_kbo_amateur_league_batch_candidate_count = count;
    if (g_kbo_amateur_league_batch_league_id == league_id
            && g_kbo_amateur_deferred_team_add_count > 0) {
        g_kbo_amateur_league_batch_last_tick = GetTickCount();
        int32_t accumulated_players = g_kbo_amateur_league_batch_player_count;
        int32_t accumulated_teams = g_kbo_amateur_league_batch_team_count;
        int ready = accumulated_teams >= count || accumulated_teams >= KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX;
        if (!ready) {
            kbo_amateur_batch_unlock();
            kbo_amateur_start_league_batch_flush_thread();
            return;
        }
        kbo_amateur_batch_unlock();
        (void)accumulated_players;
        (void)accumulated_teams;
        kbo_amateur_start_league_batch_flush_thread();
        return;
    }
    if (source_team_id == 0u) {
        kbo_amateur_batch_unlock();
        return;
    }

    if (g_kbo_amateur_league_batch_league_id != league_id
            || g_kbo_amateur_league_batch_player_count >= KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX
            || (kbo_amateur_league_batch_has_team(source_team_id)
                && g_kbo_amateur_league_batch_team_count >= count)) {
        kbo_amateur_league_batch_clear(league_id);
    }

    if (!kbo_amateur_league_batch_has_team(source_team_id)
            && g_kbo_amateur_league_batch_team_count < KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX) {
        g_kbo_amateur_league_batch_team_ids[g_kbo_amateur_league_batch_team_count++] = source_team_id;
    }

    for (int32_t i = 0; i < player_count && g_kbo_amateur_league_batch_player_count < KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX; i++) {
        uint8_t* player = (uint8_t*)players[i];
        if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || kbo_amateur_player_assignment_league_id(player) != league_id) {
            continue;
        }
        if (!kbo_amateur_league_batch_has_player(player_id)) {
            int32_t index = g_kbo_amateur_league_batch_player_count++;
            g_kbo_amateur_league_batch_players[index] = (uintptr_t)player;
            g_kbo_amateur_league_batch_source_teams[index] = 0;
            g_kbo_amateur_league_batch_player_ids[index] = player_id;
            kbo_amateur_league_batch_note_player_id(player_id);
            g_kbo_amateur_league_batch_source_team_ids[index] = source_team_id;
        }
    }

    g_kbo_amateur_league_batch_last_tick = GetTickCount();
    int32_t accumulated_players = g_kbo_amateur_league_batch_player_count;
    int32_t accumulated_teams = g_kbo_amateur_league_batch_team_count;
    int ready = accumulated_teams >= count || accumulated_teams >= KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX;
    if (!ready) {
        static volatile LONG accumulating_log_count = 0;
        LONG log_slot = InterlockedIncrement(&accumulating_log_count);
        if (log_slot <= 30 || (log_slot % 50) == 0 || accumulated_teams + 1 >= count) {
            kbo_log_runtimef(
                "amateur OR-Tools league batch accumulating league=%u teams=%d/%d players=%d latest_team=%u latest_players=%d",
                league_id,
                accumulated_teams,
                count,
                accumulated_players,
                source_team_id,
                player_count);
        } else if (log_slot == 31) {
            kbo_log_runtime_line(
                "amateur OR-Tools league batch accumulating log suppressed after 30 calls; logging every 50th call and near completion");
        }
        kbo_amateur_batch_unlock();
        kbo_amateur_start_league_batch_flush_thread();
        return;
    }
    kbo_amateur_batch_unlock();
    (void)accumulated_players;
    (void)accumulated_teams;
    kbo_amateur_start_league_batch_flush_thread();
}
