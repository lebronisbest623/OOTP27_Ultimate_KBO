/* All-Star candidate team seeding wrapper. */

#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../bootstrap/profiling/perf_probe.h"
#include "../../bootstrap/profiling/profiler.h"

#include <stdint.h>
#include <windows.h>

#include "../flags/allstar_flags.h"
#include "../allstar_league_context/allstar_league_context.h"
#include "../team_patch/allstar_team_patch.h"
#include "allstar_candidate_seed_helpers.h"
#include "../../core/dates/core_current_date.h"
#include "../../core/dates/constants/kbo_date_constants.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/league_roles/kbo_league_roles.h"
#include "../../core/logging/core_log.h"
#include "../../runtime_memory/runtime_memory.h"

static int kbo_allstar_candidate_seed_has_team(
    const uintptr_t* team_ptrs,
    const uint32_t* team_ids,
    int count,
    uintptr_t team_ptr,
    uint32_t team_id)
{
    for (int i = 0; i < count; i++) {
        if (team_ptrs[i] == team_ptr || team_ids[i] == team_id) {
            return 1;
        }
    }
    return 0;
}

__declspec(noinline) int ootp_kbo_allstar_candidate_push_filter(
    uint32_t side_index,
    uintptr_t player_ptr,
    void* candidate_vector,
    uintptr_t vector_push_back_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    static volatile LONG skip_log_count = 0;
    static volatile LONG inspect_log_count = 0;

    OotpVectorPushBack push_back = (OotpVectorPushBack)vector_push_back_ptr;
    if (push_back == NULL || candidate_vector == NULL || player_ptr == 0) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.candidate_push_filter", 0);
    }

    if (!kbo_fix_enabled() || side_index > 1u
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        push_back(candidate_vector, (void*)player_ptr);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.candidate_push_filter", 1);
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_league_role_main_league_id();
    }

    uint32_t league_year = kbo_find_current_kbo_league_year();
    if (league_year == 0u) {
        league_year = KBO_SIM_YEAR_MAX;
    }

    uint32_t team_id = 0u;
    uint8_t player_side = kbo_allstar_candidate_player_side(
        (uint8_t*)player_ptr,
        league_id,
        league_year,
        &team_id);
    uint32_t candidate_side = side_index + 1u;
    LONG inspect_index = InterlockedIncrement(&inspect_log_count);
    if (inspect_index <= 40) {
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = memory_range_readable(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET, sizeof(uint32_t))
            ? *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET)
            : 0u;
        uint32_t active_team_id = memory_range_readable(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET, sizeof(uint32_t))
            ? *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET)
            : 0u;
        uint32_t original_team_id = memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))
            ? *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET)
            : 0u;
        kbo_log_runtimef(
            "KBO all-star candidate player push inspected player=%u current=%u active=%u original=%u resolved_team=%u player_side=%u candidate_side=%u side_index=%u vector=%p",
            player_id,
            current_team_id,
            active_team_id,
            original_team_id,
            team_id,
            (uint32_t)player_side,
            candidate_side,
            side_index,
            candidate_vector);
    } else if (inspect_index == 41) {
        kbo_log_runtime_line("KBO all-star candidate player push inspect log suppressed after 40 entries");
    }

    if ((player_side == 1u || player_side == 2u) && player_side != candidate_side) {
        LONG log_index = InterlockedIncrement(&skip_log_count);
        if (log_index <= 40) {
            uint32_t player_id = *(uint32_t*)((uint8_t*)player_ptr + OOTP27_PLAYER_ID_OFFSET);
            kbo_log_runtimef(
                "KBO all-star candidate player push skipped player=%u team=%u player_side=%u candidate_side=%u side_index=%u",
                player_id,
                team_id,
                (uint32_t)player_side,
                candidate_side,
                side_index);
        } else if (log_index == 41) {
            kbo_log_runtime_line("KBO all-star candidate player push skip log suppressed after 40 entries");
        }
        KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.candidate_push_filter", 0);
    }

    KBO_HOOK_PROFILE_PAUSE(profile_hook);
    push_back(candidate_vector, (void*)player_ptr);
    KBO_HOOK_PROFILE_RESUME(profile_hook);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.candidate_push_filter", 1);
}

__declspec(noinline) int ootp_kbo_seed_single_division_allstar_candidate_teams(
    uintptr_t league_ptr,
    void* left_team_vector,
    void* right_team_vector,
    uintptr_t vector_push_back_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    static volatile LONG perf_total = 0;
    static volatile LONG perf_last = 0;
    static volatile LONG perf_ms = 0;
    static volatile LONG perf_max = 0;
    static volatile LONG perf_tick = 0;
    DWORD perf_start = GetTickCount();
    kbo_perf_probe_record(
        "allstar_candidate_team_split_enter",
        &perf_total,
        &perf_last,
        &perf_ms,
        &perf_max,
        &perf_tick,
        0);

    if (!kbo_fix_enabled()) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.candidate_team_seed", 0);
    }
    if (left_team_vector == NULL || right_team_vector == NULL || vector_push_back_ptr == 0) {
        kbo_log_runtimef("KBO allstar candidate seed: return=0 reason=null_vectors left=%p right=%p push=%p", left_team_vector, right_team_vector, (void*)vector_push_back_ptr);
        KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.candidate_team_seed", 0);
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_league_role_main_league_id();
    }

    uint32_t league_year = kbo_find_current_kbo_league_year();
    if (league_year == 0u && league_ptr != 0
            && memory_range_readable((void*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET), sizeof(uint32_t))) {
        uint32_t probe = *(uint32_t*)(league_ptr + OOTP27_KBO_LEAGUE_YEAR_OFFSET);
        if (probe >= KBO_SEASON_YEAR_MIN && probe <= KBO_SIM_YEAR_MAX) {
            league_year = probe;
        }
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0) {
        kbo_log_runtime_line("KBO allstar candidate seed: return=0 reason=no_global_db");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.candidate_team_seed", 0);
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > 10000 || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        kbo_log_runtimef("KBO allstar candidate seed: return=0 reason=no_team_vector league_id=%u year=%u", league_id, league_year);
        KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.candidate_team_seed", 0);
    }

    ensure_kbo_allstar_team_ids(league_ptr, "allstar_candidate_team_split");

    uintptr_t eligible_team_ptrs[128] = {0};
    uint32_t eligible_team_ids[128] = {0};
    uint8_t eligible_sides[128] = {0};
    int eligible_count = 0;
    int side_rule_count = 0;
    int unknown_side_count = 0;
    int duplicate_team_count = 0;
    int exhibition_team_count = 0;
    uint32_t left_ids[64] = {0};
    uint32_t right_ids[64] = {0};
    int left_count = 0;
    int right_count = 0;
    OotpVectorPushBack push_back = (OotpVectorPushBack)vector_push_back_ptr;

    for (int32_t i = 0; i < team_count; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0 || team[OOTP27_KBO_TEAM_DELETED_OFFSET + 1u] != 0) {
            continue;
        }
        if (!kbo_allstar_team_matches_league_ids(team, league_id, 0u)) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id == 0 || team_id > (uint32_t)team_count) {
            continue;
        }
        if (kbo_allstar_candidate_seed_is_exhibition_team(team)) {
            exhibition_team_count++;
            continue;
        }
        if (kbo_allstar_candidate_seed_has_team(
                eligible_team_ptrs,
                eligible_team_ids,
                eligible_count,
                team_ptr,
                team_id)) {
            duplicate_team_count++;
            continue;
        }
        if (eligible_count >= (int)(sizeof(eligible_team_ptrs) / sizeof(eligible_team_ptrs[0]))) {
            continue;
        }

        uint8_t side = kbo_allstar_side_for_team(team, league_year);
        eligible_team_ptrs[eligible_count] = team_ptr;
        eligible_team_ids[eligible_count] = team_id;
        eligible_sides[eligible_count] = side;
        eligible_count++;
        if (side == 1 || side == 2) {
            side_rule_count++;
        } else {
            unknown_side_count++;
        }
    }

    for (int i = 0; i < eligible_count; i++) {
        if (eligible_sides[i] == 1) {
            left_count++;
        } else if (eligible_sides[i] == 2) {
            right_count++;
        }
    }

    int used_csv_split = left_count > 0 && right_count > 0;
    left_count = 0;
    right_count = 0;

    for (int i = 0; i < eligible_count; i++) {
        int send_left = 0;
        if (used_csv_split) {
            if (eligible_sides[i] == 0) {
                continue;
            }
            send_left = eligible_sides[i] == 1;
        } else {
            send_left = ((left_count + right_count) & 1) == 0;
        }

        if (send_left) {
            KBO_HOOK_PROFILE_PAUSE(profile_hook);
            push_back(left_team_vector, (void*)eligible_team_ptrs[i]);
            KBO_HOOK_PROFILE_RESUME(profile_hook);
            if (left_count < (int)(sizeof(left_ids) / sizeof(left_ids[0]))) {
                left_ids[left_count] = eligible_team_ids[i];
            }
            left_count++;
        } else {
            KBO_HOOK_PROFILE_PAUSE(profile_hook);
            push_back(right_team_vector, (void*)eligible_team_ptrs[i]);
            KBO_HOOK_PROFILE_RESUME(profile_hook);
            if (right_count < (int)(sizeof(right_ids) / sizeof(right_ids[0]))) {
                right_ids[right_count] = eligible_team_ids[i];
            }
            right_count++;
        }
    }

    if (left_count <= 0 || right_count <= 0) {
        kbo_log_runtimef("KBO allstar candidate seed: return=0 reason=empty_side league_id=%u year=%u eligible=%d left=%d right=%d used_csv=%d", league_id, league_year, eligible_count, left_count, right_count, used_csv_split);
        KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.candidate_team_seed", 0);
    }

    /* Cache the league pointer so kbo_find_allstar_league_ptr can use it, and keep
     * game_flag/rules_flag/auto_schedule immediately so the voting ballot shows candidates.
     * The retry thread may have already given up by the time the candidate split fires,
     * so this is the most reliable path to ensure game_flag=1 before voting is checked.
     *
     * The raw flag path accepts the same league pointer that OOTP passed to this hook.
     * enable_kbo_allstar_flags remains as a strict-context fallback. */
    if (league_ptr != 0) {
        InterlockedCompareExchangePointer(
            (PVOID volatile*)&g_allstar_schedule_import_league_ptr,
            (PVOID)league_ptr,
            NULL);
        if (!enable_kbo_allstar_raw_flags_if_kbo_context(league_ptr, "candidate_split_raw")) {
            enable_kbo_allstar_flags(league_ptr, "candidate_split");
        }

    }

    LONG log_index = InterlockedIncrement(&g_allstar_candidate_seed_log_count);
    if (log_index <= 20) {
        kbo_log_runtimef(
            "seeded KBO single-division all-star candidate teams league_id=%u year=%u teams_scanned=%d left_count=%d first_left=%u right_count=%d first_right=%u",
            league_id,
            league_year,
            team_count,
            left_count,
            left_ids[0],
            right_count,
            right_ids[0]);
        kbo_log_runtimef(
            "KBO all-star candidate team split source=allstar_teams.csv league_id=%u eligible=%d rule_sides=%d unknown_sides=%d exhibition_skipped=%d duplicate_skipped=%d mode=%s",
            league_id,
            eligible_count,
            side_rule_count,
            unknown_side_count,
            exhibition_team_count,
            duplicate_team_count,
            used_csv_split ? "csv" : "fallback_alternating");
    }

    static volatile LONG perf_success_total = 0;
    static volatile LONG perf_success_last = 0;
    static volatile LONG perf_success_ms = 0;
    static volatile LONG perf_success_max = 0;
    static volatile LONG perf_success_tick = 0;
    kbo_perf_probe_record(
        "allstar_candidate_team_split_success",
        &perf_success_total,
        &perf_success_last,
        &perf_success_ms,
        &perf_success_max,
        &perf_success_tick,
        GetTickCount() - perf_start);

    KBO_HOOK_PROFILE_RETURN(profile_hook, "allstar.candidate_team_seed", 1);
}
