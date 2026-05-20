#include "../internal/amateur_assignment_internal.h"
#include "../../runtime_memory/runtime_memory.h"

uint32_t kbo_amateur_assignment_candidate_weight(
    uint32_t league_id,
    int32_t quality_score,
    int assignment_tier,
    int32_t target,
    uint8_t reputation,
    int32_t player_count,
    int32_t target_player_count)
{
    int32_t target_max_players = kbo_amateur_assignment_target_max_players(league_id);
    if (target_max_players > 0 && player_count >= target_max_players) {
        return 0u;
    }
    int player_tier = kbo_amateur_assignment_player_tier(league_id, quality_score);
    if (assignment_tier >= 0) {
        player_tier = assignment_tier;
    }
    int team_tier = kbo_amateur_assignment_team_tier(league_id, reputation);
    if (!kbo_amateur_assignment_tier_allowed(player_tier, team_tier)) {
        return 0u;
    }
    int32_t distance = abs((int32_t)reputation - target);
    int32_t base = 96 - (distance * 4);
    if (base < 8) {
        base = 8;
    }
    int tier_delta = team_tier - player_tier;
    int tier_fit = abs(tier_delta);
    int32_t tier_multiplier = 100;
    if (tier_fit == 0) {
        tier_multiplier = 140;
    } else if (tier_delta > 0) {
        tier_multiplier = 65;
    } else {
        tier_multiplier = player_tier >= 4 ? 30 : 45;
    }

    int32_t weight_value = (base * tier_multiplier) / 100;
    if (target_player_count >= 0 && player_count > target_player_count) {
        int32_t overage = player_count - target_player_count;
        weight_value -= overage * 8;
    }
    if (target_max_players > 0 && player_count >= target_max_players - 2) {
        weight_value /= 2;
    }
    uint32_t weight = (uint32_t)(weight_value > 0 ? weight_value : 1);
    if (weight == 0u) {
        weight = 1u;
    }
    return weight;
}

int kbo_amateur_assignment_collect_candidates(
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int max_candidates)
{
    if (league_id == 0u || candidates == NULL || max_candidates <= 0) {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return 0;
    }

    uintptr_t team_vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (team_vector == 0 || team_count <= 0 || team_count > KBO_RUNTIME_MAX_TEAM_VECTOR_COUNT
            || !memory_range_readable((void*)team_vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    int found = 0;
    for (int32_t i = 0; i < team_count && found < max_candidates; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(team_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint8_t* team = (uint8_t*)team_ptr;
        if (team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint8_t reputation = kbo_amateur_default_team_reputation();
        if (team_id == 0u || !kbo_find_amateur_team_reputation_by_memory_team(league_id, team, &reputation)) {
            continue;
        }

        candidates[found].team = team;
        candidates[found].team_id = team_id;
        candidates[found].reputation = reputation;
        candidates[found].player_count = 0;
        candidates[found].hitter_count = 0;
        candidates[found].pitcher_count = 0;
        candidates[found].catcher_count = 0;
        candidates[found].infielder_count = 0;
        candidates[found].outfielder_count = 0;
        candidates[found].first_base_count = 0;
        candidates[found].second_base_count = 0;
        candidates[found].third_base_count = 0;
        candidates[found].shortstop_count = 0;
        candidates[found].left_field_count = 0;
        candidates[found].center_field_count = 0;
        candidates[found].right_field_count = 0;
        candidates[found].designated_hitter_count = 0;
        found++;
    }
    return found;
}

void kbo_amateur_assignment_refresh_player_counts(
    uint32_t league_id,
    KboAmateurAssignmentCandidate* candidates,
    int candidate_count)
{
    if (league_id == 0u || candidates == NULL || candidate_count <= 0) {
        return;
    }

    for (int i = 0; i < candidate_count; i++) {
        candidates[i].player_count = 0;
        candidates[i].hitter_count = 0;
        candidates[i].pitcher_count = 0;
        candidates[i].catcher_count = 0;
        candidates[i].infielder_count = 0;
        candidates[i].outfielder_count = 0;
        candidates[i].first_base_count = 0;
        candidates[i].second_base_count = 0;
        candidates[i].third_base_count = 0;
        candidates[i].shortstop_count = 0;
        candidates[i].left_field_count = 0;
        candidates[i].center_field_count = 0;
        candidates[i].right_field_count = 0;
        candidates[i].designated_hitter_count = 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return;
    }
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        if (!kbo_amateur_player_age_eligible(league_id, age)) {
            continue;
        }
        uint32_t team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (team_id == 0u) {
            team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        }
        if (team_id == 0u) {
            continue;
        }
        for (int c = 0; c < candidate_count; c++) {
            if (candidates[c].team_id == team_id) {
                candidates[c].player_count++;
                if (kbo_amateur_player_is_hitter(player)) {
                    candidates[c].hitter_count++;
                }
                switch (kbo_amateur_player_position_bucket(player)) {
                case 0: candidates[c].pitcher_count++; break;
                case 1: candidates[c].catcher_count++; break;
                case 2: candidates[c].first_base_count++; candidates[c].infielder_count++; break;
                case 3: candidates[c].second_base_count++; candidates[c].infielder_count++; break;
                case 4: candidates[c].third_base_count++; candidates[c].infielder_count++; break;
                case 5: candidates[c].shortstop_count++; candidates[c].infielder_count++; break;
                case 6: candidates[c].left_field_count++; candidates[c].outfielder_count++; break;
                case 7: candidates[c].center_field_count++; candidates[c].outfielder_count++; break;
                case 8: candidates[c].right_field_count++; candidates[c].outfielder_count++; break;
                default: break;
                }
                break;
            }
        }
    }
}

void kbo_amateur_assignment_note_player_count_delta(uint32_t league_id, uint32_t team_id, uint8_t* player, int32_t delta)
{
    if (team_id == 0u || delta == 0) {
        return;
    }
    int* count_ptr = kbo_amateur_assignment_count_ptr_for_league(league_id);
    KboAmateurAssignmentCandidate* candidates = kbo_amateur_assignment_cache_for_league(league_id);
    if (count_ptr == NULL || candidates == NULL || *count_ptr <= 0) {
        return;
    }

    kbo_lock_amateur_assignment_candidates();
    int count = *count_ptr;
    for (int i = 0; i < count; i++) {
        if (candidates[i].team_id == team_id) {
            candidates[i].player_count += delta;
            if (candidates[i].player_count < 0) {
                candidates[i].player_count = 0;
            }
            if (kbo_amateur_player_is_hitter(player)) {
                candidates[i].hitter_count += delta;
                if (candidates[i].hitter_count < 0) {
                    candidates[i].hitter_count = 0;
                }
            }
            int bucket = kbo_amateur_player_position_bucket(player);
            int32_t* bucket_count = NULL;
            switch (bucket) {
            case 0: bucket_count = &candidates[i].pitcher_count; break;
            case 1: bucket_count = &candidates[i].catcher_count; break;
            case 2: bucket_count = &candidates[i].first_base_count; candidates[i].infielder_count += delta; break;
            case 3: bucket_count = &candidates[i].second_base_count; candidates[i].infielder_count += delta; break;
            case 4: bucket_count = &candidates[i].third_base_count; candidates[i].infielder_count += delta; break;
            case 5: bucket_count = &candidates[i].shortstop_count; candidates[i].infielder_count += delta; break;
            case 6: bucket_count = &candidates[i].left_field_count; candidates[i].outfielder_count += delta; break;
            case 7: bucket_count = &candidates[i].center_field_count; candidates[i].outfielder_count += delta; break;
            case 8: bucket_count = &candidates[i].right_field_count; candidates[i].outfielder_count += delta; break;
            default: break;
            }
            if (bucket_count != NULL) {
                *bucket_count += delta;
                if (*bucket_count < 0) {
                    *bucket_count = 0;
                }
            }
            if (candidates[i].infielder_count < 0) {
                candidates[i].infielder_count = 0;
            }
            if (candidates[i].outfielder_count < 0) {
                candidates[i].outfielder_count = 0;
            }
            break;
        }
    }
    kbo_unlock_amateur_assignment_candidates();
}

int kbo_amateur_assignment_get_cached_candidates(
    uint32_t league_id,
    KboAmateurAssignmentCandidate** out_candidates)
{
    if (out_candidates != NULL) {
        *out_candidates = NULL;
    }
    if (league_id != KBO_HIGH_SCHOOL_LEAGUE_ID && league_id != KBO_COLLEGE_LEAGUE_ID) {
        return 0;
    }

    KboAmateurAssignmentCandidate* cache = kbo_amateur_assignment_cache_for_league(league_id);
    int* count_ptr = kbo_amateur_assignment_count_ptr_for_league(league_id);
    if (cache == NULL || count_ptr == NULL) {
        return 0;
    }

    if (*count_ptr >= 0) {
        if (out_candidates != NULL) {
            *out_candidates = cache;
        }
        return *count_ptr;
    }

    kbo_lock_amateur_assignment_candidates();
    if (*count_ptr < 0) {
        memset(cache, 0, sizeof(KboAmateurAssignmentCandidate) * KBO_AMATEUR_ASSIGNMENT_TEAM_MAX);
        *count_ptr = kbo_amateur_assignment_collect_candidates(
            league_id,
            cache,
            KBO_AMATEUR_ASSIGNMENT_TEAM_MAX);
        kbo_amateur_assignment_refresh_player_counts(league_id, cache, *count_ptr);
        kbo_log_runtimef(
            "amateur assignment candidate cache built league=%u teams=%d counts=refreshed",
            league_id,
            *count_ptr);
    }
    int count = *count_ptr;
    kbo_unlock_amateur_assignment_candidates();

    if (out_candidates != NULL) {
        *out_candidates = cache;
    }
    return count;
}

