#include "ortools/amateur_assignment_ortools.h"
#include "../../core/optimizer/kbo_optimizer.h"
#include "../../core/core_flags/keys/runtime_flag_keys.generated.h"

KboAmateurBatchAssignment g_kbo_amateur_batch_assignments[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
LONG g_kbo_amateur_batch_assignment_count = 0;
KboLock g_kbo_amateur_batch_assignment_lock = KBO_LOCK_INIT;
uintptr_t g_kbo_amateur_league_batch_players[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
uintptr_t g_kbo_amateur_league_batch_source_teams[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
uint32_t g_kbo_amateur_league_batch_team_ids[KBO_AMATEUR_LEAGUE_BATCH_TEAM_MAX];
uint32_t g_kbo_amateur_league_batch_league_id = 0u;
int32_t g_kbo_amateur_league_batch_player_count = 0;
int32_t g_kbo_amateur_league_batch_team_count = 0;
DWORD g_kbo_amateur_league_batch_last_tick = 0u;
volatile LONG g_kbo_amateur_league_batch_flush_thread_started = 0;
KboAmateurDeferredTeamAdd g_kbo_amateur_deferred_team_adds[KBO_AMATEUR_LEAGUE_BATCH_PLAYER_MAX];
int32_t g_kbo_amateur_deferred_team_add_count = 0;
static volatile LONG g_kbo_amateur_disable_reroute_cached = -1;
static volatile LONG g_kbo_amateur_disable_reroute_tick = 0;
static volatile LONG g_kbo_amateur_disable_deferred_cached = -1;
static volatile LONG g_kbo_amateur_disable_deferred_tick = 0;
static volatile LONG g_kbo_amateur_verbose_log_cached = -1;
static volatile LONG g_kbo_amateur_verbose_log_tick = 0;

static int kbo_amateur_cached_bool_flag(
    const char* file_name,
    volatile LONG* cached_value,
    volatile LONG* cached_tick,
    DWORD ttl_ms)
{
    DWORD now = GetTickCount();
    LONG value = *cached_value;
    LONG tick = *cached_tick;
    if (value >= 0 && now - (DWORD)tick < ttl_ms) {
        return value != 0;
    }

    int fresh = read_kbo_localappdata_flag_file(file_name) ? 1 : 0;
    InterlockedExchange(cached_value, fresh);
    InterlockedExchange(cached_tick, (LONG)now);
    return fresh;
}

int kbo_amateur_reroute_disabled_cached(void)
{
    return kbo_amateur_cached_bool_flag(
        KBO_RUNTIME_FLAG_DISABLE_AMATEUR_ASSIGNMENT_REROUTE_FILE,
        &g_kbo_amateur_disable_reroute_cached,
        &g_kbo_amateur_disable_reroute_tick,
        1000u);
}

int kbo_amateur_deferred_add_disabled_cached(void)
{
    return kbo_amateur_cached_bool_flag(
        KBO_RUNTIME_FLAG_DISABLE_AMATEUR_ASSIGNMENT_DEFERRED_ADD_FILE,
        &g_kbo_amateur_disable_deferred_cached,
        &g_kbo_amateur_disable_deferred_tick,
        1000u);
}

int kbo_amateur_verbose_log_enabled_cached(void)
{
    return kbo_amateur_cached_bool_flag(
        KBO_RUNTIME_FLAG_ENABLE_AMATEUR_ASSIGNMENT_VERBOSE_LOG_FILE,
        &g_kbo_amateur_verbose_log_cached,
        &g_kbo_amateur_verbose_log_tick,
        5000u);
}

void kbo_amateur_batch_lock(void)
{
    kbo_lock_enter(&g_kbo_amateur_batch_assignment_lock);
}

void kbo_amateur_batch_unlock(void)
{
    kbo_lock_leave(&g_kbo_amateur_batch_assignment_lock);
}

uint32_t kbo_amateur_batch_lookup(uint32_t league_id, uint32_t player_id)
{
    uint32_t target_team_id = 0u;
    kbo_amateur_batch_lock();
    LONG count = g_kbo_amateur_batch_assignment_count;
    for (LONG i = 0; i < count; i++) {
        if (g_kbo_amateur_batch_assignments[i].league_id == league_id
                && g_kbo_amateur_batch_assignments[i].player_id == player_id) {
            target_team_id = g_kbo_amateur_batch_assignments[i].target_team_id;
            break;
        }
    }
    kbo_amateur_batch_unlock();
    return target_team_id;
}

uint8_t* kbo_choose_amateur_assignment_team_ortools(
    uint8_t* player,
    uint32_t league_id,
    uint32_t current_team_id,
    uint8_t current_reputation,
    int32_t quality_score,
    uint8_t* out_target_reputation)
{
    if (player == NULL || league_id == 0u || current_team_id == 0u || quality_score <= 0) {
        return NULL;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 1 || candidates == NULL) {
        return NULL;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t batch_target_team_id = kbo_amateur_batch_lookup(league_id, player_id);
    if (batch_target_team_id != 0u && batch_target_team_id != current_team_id) {
        for (int i = 0; i < count; i++) {
            if (candidates[i].team_id == batch_target_team_id) {
                if (out_target_reputation != NULL) {
                    *out_target_reputation = candidates[i].reputation;
                }
                return candidates[i].team;
            }
        }
    }

    int32_t target = kbo_amateur_assignment_target_reputation(league_id, quality_score);
    if (out_target_reputation != NULL) {
        *out_target_reputation = (uint8_t)target;
    }
    int player_tier = kbo_amateur_assignment_player_tier(league_id, quality_score);

    char request_path[MAX_PATH * 3] = {0};
    char result_path[MAX_PATH * 3] = {0};
    if (!kbo_get_save_scoped_data_file("amateur_assignment_ortools_request.csv", request_path, sizeof(request_path))
            || !kbo_get_save_scoped_data_file("amateur_assignment_ortools_result.csv", result_path, sizeof(result_path))) {
        static volatile LONG path_fail_log_count = 0;
        if (InterlockedIncrement(&path_fail_log_count) <= 5) {
            kbo_log_runtime_line("amateur OR-Tools save-scoped request/result path unavailable");
        }
        return NULL;
    }

    if (!kbo_amateur_ortools_write_request(
            request_path,
            player,
            league_id,
            current_team_id,
            current_reputation,
            quality_score,
            player_tier,
            target,
            candidates,
            count)) {
        return NULL;
    }
    if (!kbo_optimizer_run_mode("amateur_assignment", request_path, result_path, 5000u)) {
        return NULL;
    }

    uint32_t target_team_id = kbo_amateur_ortools_read_result(result_path);
    if (target_team_id == 0u || target_team_id == current_team_id) {
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == target_team_id) {
            static volatile LONG log_count = 0;
            LONG slot = InterlockedIncrement(&log_count);
            if (slot <= 50 || kbo_amateur_verbose_log_enabled_cached()) {
                kbo_log_runtimef(
                    "amateur OR-Tools assignment player=%u league=%u score=%d target_rep=%d team=%u(rep=%u)->%u(rep=%u)",
                    player_id,
                    league_id,
                    quality_score,
                    target,
                    current_team_id,
                    (uint32_t)current_reputation,
                    target_team_id,
                    (uint32_t)candidates[i].reputation);
            }
            return candidates[i].team;
        }
    }
    return NULL;
}
