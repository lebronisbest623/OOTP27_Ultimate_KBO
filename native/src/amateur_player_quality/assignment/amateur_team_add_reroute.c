#include "../internal/amateur_assignment_internal.h"
#include "../../bootstrap/profiling/profiler.h"
#include <string.h>
static volatile LONG g_kbo_amateur_reroute_debug_csv_cached = -1;
static volatile LONG g_kbo_amateur_reroute_debug_csv_tick = 0;

static int kbo_amateur_reroute_debug_csv_enabled_cached(void)
{
    DWORD now = GetTickCount();
    LONG value = g_kbo_amateur_reroute_debug_csv_cached;
    LONG tick = g_kbo_amateur_reroute_debug_csv_tick;
    if (value >= 0 && now - (DWORD)tick < 5000u) {
        return value != 0;
    }

    int fresh = read_kbo_localappdata_flag_file("enable_amateur_assignment_debug_csv.txt") ? 1 : 0;
    InterlockedExchange(&g_kbo_amateur_reroute_debug_csv_cached, fresh);
    InterlockedExchange(&g_kbo_amateur_reroute_debug_csv_tick, (LONG)now);
    return fresh;
}

int kbo_amateur_assignment_find_candidate_info(
    uint32_t league_id,
    uint32_t team_id,
    uint8_t* out_reputation,
    int* out_tier,
    int32_t* out_player_count,
    int32_t* out_hitter_count)
{
    if (out_reputation != NULL) { *out_reputation = 0u; }
    if (out_tier != NULL) { *out_tier = -1; }
    if (out_player_count != NULL) { *out_player_count = -1; }
    if (out_hitter_count != NULL) { *out_hitter_count = -1; }
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }

    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 0 || candidates == NULL) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == team_id) {
            if (out_reputation != NULL) {
                *out_reputation = candidates[i].reputation;
            }
            if (out_tier != NULL) {
                *out_tier = kbo_amateur_assignment_team_tier(league_id, candidates[i].reputation);
            }
            if (out_player_count != NULL) {
                *out_player_count = candidates[i].player_count;
            }
            if (out_hitter_count != NULL) {
                *out_hitter_count = candidates[i].hitter_count;
            }
            return 1;
        }
    }
    return 0;
}

static uint8_t* kbo_amateur_assignment_candidate_team_ptr(uint32_t league_id, uint32_t team_id)
{
    KboAmateurAssignmentCandidate* candidates = NULL;
    int count = kbo_amateur_assignment_get_cached_candidates(league_id, &candidates);
    if (count <= 0 || candidates == NULL || team_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == team_id) {
            return candidates[i].team;
        }
    }
    return NULL;
}

uintptr_t kbo_amateur_team_add_player_reroute_before_original(uintptr_t team_ptr, uintptr_t player_ptr, const char* source)
{
    if (kbo_amateur_reroute_disabled_cached()
            || team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        return team_ptr;
    }

    if (source != NULL && strcmp(source, "team_add_player_before_original") == 0) {
        static volatile LONG post_add_log_count = 0;
        LONG post_add_slot = InterlockedIncrement(&post_add_log_count);
        if (post_add_slot <= 5 || kbo_amateur_verbose_log_enabled_cached()) {
            kbo_log_runtimef(
                "amateur assignment pre-reroute skipped source=%s reason=post_original_batch_reassignment",
                source);
        } else if (post_add_slot == 6) {
            kbo_log_runtime_line("amateur assignment pre-reroute skip log suppressed after 5 calls; create enable_amateur_assignment_verbose_log.txt for full logging");
        }
        return team_ptr;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t player_league_id = kbo_amateur_player_assignment_league_id(player);
    uint32_t team_league_id =
        (player_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID || player_league_id == KBO_COLLEGE_LEAGUE_ID)
            ? player_league_id
            : kbo_resolve_amateur_assignment_league_id_for_team_and_player(team, player);
    if (team_league_id == 0u) {
        return team_ptr;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint32_t source_team_id = team_id;
    uint32_t player_team_id = kbo_amateur_player_assignment_team_id(player);
    uint8_t current_reputation = kbo_amateur_default_team_reputation();
    int current_reputation_found = 0;
    if (player_team_id != 0u
            && kbo_amateur_assignment_find_candidate_info(
                team_league_id,
                player_team_id,
                &current_reputation,
                NULL,
                NULL,
                NULL)) {
        source_team_id = player_team_id;
        current_reputation_found = 1;
    }
    if (player_id == 0u || source_team_id == 0u || !kbo_amateur_player_age_eligible(team_league_id, age)
            || kbo_player_is_draft_pool_candidate(player)
            || kbo_amateur_assignment_already_processed(player_id, source_team_id)) {
        return team_ptr;
    }

    if (!current_reputation_found
            && !kbo_find_amateur_team_reputation_by_memory_team(team_league_id, team, &current_reputation)) {
        return team_ptr;
    }

    int32_t quality_score = kbo_amateur_quality_score(player);
    int player_tier = kbo_amateur_assignment_player_tier(team_league_id, quality_score);

    if (team_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID || team_league_id == KBO_COLLEGE_LEAGUE_ID) {
        int32_t current_player_count = -1;
        int32_t current_hitter_count = -1;
        kbo_amateur_assignment_find_candidate_info(
            team_league_id,
            source_team_id,
            NULL,
            NULL,
            &current_player_count,
            &current_hitter_count);
        const KboAmateurPlayerQualityPolicy* policy = kbo_amateur_player_quality_policy();
        int32_t source_min_players = team_league_id == KBO_HIGH_SCHOOL_LEAGUE_ID
            ? policy->high_school_assignment_source_min_players
            : policy->college_assignment_source_min_players;
        if (current_player_count >= 0 && current_player_count < source_min_players) {
            return team_ptr;
        }
        if (kbo_amateur_player_is_hitter(player)
                && current_hitter_count >= 0
                && current_hitter_count < policy->assignment_source_min_hitters) {
            return team_ptr;
        }
    }

    uint8_t target_reputation = 0u;
    uint8_t* target_team = kbo_choose_amateur_assignment_team_ortools(
        player,
        team_league_id,
        source_team_id,
        current_reputation,
        quality_score,
        &target_reputation);
    if (target_team == NULL || target_team == team) {
        uint8_t* source_team = kbo_amateur_assignment_candidate_team_ptr(team_league_id, source_team_id);
        int from_tier = kbo_amateur_assignment_team_tier(team_league_id, current_reputation);
        int32_t from_player_count = -1;
        kbo_amateur_assignment_find_candidate_info(
            team_league_id,
            source_team_id,
            NULL,
            NULL,
            &from_player_count,
            NULL);
        kbo_amateur_assignment_append_debug_csv(
            target_team == team ? "pre_keep_same_tier_slot" : "pre_keep_no_candidate",
            source,
            player_id,
            team_league_id,
            age,
            quality_score,
            player_tier,
            source_team_id,
            current_reputation,
            from_tier,
            from_player_count,
            source_team_id,
            current_reputation,
            from_tier,
            from_player_count,
            from_player_count,
            target_reputation,
            -1,
            0u,
            0u);
        if (source_team != NULL && source_team != team) {
            return (uintptr_t)source_team;
        }
        return team_ptr;
    }

    uint32_t target_team_id = *(uint32_t*)(target_team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint8_t actual_target_reputation = kbo_amateur_default_team_reputation();
    kbo_find_amateur_team_reputation_by_memory_team(team_league_id, target_team, &actual_target_reputation);
    int from_tier = kbo_amateur_assignment_team_tier(team_league_id, current_reputation);
    int to_tier = kbo_amateur_assignment_team_tier(team_league_id, actual_target_reputation);
    int32_t from_player_count = -1;
    int32_t to_player_count = -1;
    kbo_amateur_assignment_find_candidate_info(
        team_league_id,
        source_team_id,
            NULL,
            NULL,
            &from_player_count,
            NULL);
        kbo_amateur_assignment_find_candidate_info(
            team_league_id,
            target_team_id,
            NULL,
            NULL,
            &to_player_count,
            NULL);
    int32_t target_player_count = to_player_count;

    kbo_amateur_assignment_append_debug_csv(
        "pre_reroute_decision",
        source,
        player_id,
        team_league_id,
        age,
        quality_score,
        player_tier,
        source_team_id,
        current_reputation,
        from_tier,
        from_player_count,
        target_team_id,
        actual_target_reputation,
        to_tier,
        to_player_count,
        target_player_count,
        target_reputation,
        -1,
        0u,
        0u);

    static volatile LONG reroute_before_log_count = 0;
    LONG slot = InterlockedIncrement(&reroute_before_log_count);
    int verbose_assignment_log = kbo_amateur_verbose_log_enabled_cached();
    if (verbose_assignment_log || slot <= 30) {
        kbo_log_runtimef(
            "amateur assignment pre-reroute team-add source=%s player=%u league=%u age=%d score=%d target_rep=%u team=%u(rep=%u)->%u(rep=%u)",
            source != NULL ? source : "",
            player_id,
            team_league_id,
            (int)age,
            quality_score,
            (uint32_t)target_reputation,
            source_team_id,
            (uint32_t)current_reputation,
            target_team_id,
            (uint32_t)actual_target_reputation);
    } else if (slot == 31) {
        kbo_log_runtime_line("amateur assignment pre-reroute log suppressed after 30 players; create enable_amateur_assignment_verbose_log.txt for full logging");
    }

    return (uintptr_t)target_team;
}

void kbo_amateur_team_add_player_note_original_success(uintptr_t team_ptr, uintptr_t player_ptr, const char* source, int original_result)
{
    KBO_PROFILE_BEGIN(profile_note_success);
    if (team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        KBO_PROFILE_END(profile_note_success, "amateur.note_success.precheck_reject");
        return;
    }
    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_league_id = kbo_amateur_player_assignment_league_id(player);
    if (player_league_id != KBO_HIGH_SCHOOL_LEAGUE_ID && player_league_id != KBO_COLLEGE_LEAGUE_ID) {
        KBO_PROFILE_END(profile_note_success, "amateur.note_success.non_amateur_player");
        return;
    }
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = kbo_resolve_amateur_assignment_league_id_for_team_and_player(team, player);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (team_league_id == 0u
            || team_id == 0u
            || player_id == 0u) {
        KBO_PROFILE_END(profile_note_success, "amateur.note_success.bad_ids");
        return;
    }

    KBO_PROFILE_BEGIN(profile_already_processed);
    int already_processed = kbo_amateur_assignment_already_processed(player_id, team_id);
    KBO_PROFILE_END(profile_already_processed, "amateur.note_success.already_processed_check");
    if (already_processed) {
        KBO_PROFILE_END(profile_note_success, "amateur.note_success.already_processed");
        return;
    }

    KBO_PROFILE_BEGIN(profile_mark_processed);
    kbo_amateur_assignment_mark_processed(player_id, team_id);
    KBO_PROFILE_END(profile_mark_processed, "amateur.note_success.mark_processed");

    KBO_PROFILE_BEGIN(profile_count_delta);
    kbo_amateur_assignment_note_player_count_delta(team_league_id, team_id, player, 1);
    KBO_PROFILE_END(profile_count_delta, "amateur.note_success.count_delta");

    if (!kbo_amateur_reroute_debug_csv_enabled_cached()) {
        KBO_PROFILE_END(profile_note_success, "amateur.note_success.no_debug_csv");
        return;
    }

    KBO_PROFILE_BEGIN(profile_debug_csv);
    uint8_t reputation = 0u;
    int team_tier = -1;
    int32_t player_count = -1;
    kbo_amateur_assignment_find_candidate_info(
        team_league_id,
        team_id,
        &reputation,
        &team_tier,
        &player_count,
        NULL);
    int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    int32_t quality_score = kbo_amateur_quality_score(player);
    int player_tier = kbo_amateur_assignment_player_tier(team_league_id, quality_score);
    uint32_t after_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    kbo_amateur_assignment_append_debug_csv(
        "original_success",
        source,
        player_id,
        team_league_id,
        age,
        quality_score,
        player_tier,
        team_id,
        reputation,
        team_tier,
        player_count,
        team_id,
        reputation,
        team_tier,
        player_count,
        player_count,
        reputation,
        original_result,
        after_team_id,
        after_league_id);
    KBO_PROFILE_END(profile_debug_csv, "amateur.note_success.debug_csv");
    KBO_PROFILE_END(profile_note_success, "amateur.note_success.debug_csv_done");
}

