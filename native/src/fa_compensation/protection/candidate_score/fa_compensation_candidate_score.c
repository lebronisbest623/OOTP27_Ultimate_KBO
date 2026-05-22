#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_compensation_candidate_score.h"

#include <stdio.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../policy/fa_compensation_protection_policy.h"

enum {
    KBO_FA_TEAM_ROLE_COUNT_CACHE_SIZE = 64,
    KBO_FA_ROLE_BUCKET_COUNT = 5
};

typedef struct KboFaTeamRoleCountCacheEntry {
    uint32_t team_id;
    uintptr_t player_vector;
    int32_t player_count;
    int counts[KBO_FA_ROLE_BUCKET_COUNT];
    uint8_t valid;
} KboFaTeamRoleCountCacheEntry;

static KboFaTeamRoleCountCacheEntry g_kbo_fa_team_role_count_cache[KBO_FA_TEAM_ROLE_COUNT_CACHE_SIZE];

static int kbo_fa_score_player_vector_readable(uintptr_t player_vector, int32_t player_count)
{
    return player_vector != 0 && player_count > 0 && player_count <= KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT
        && (SIZE_T)player_count <= ((SIZE_T)-1 / sizeof(uintptr_t))
        && memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t));
}

int kbo_fa_role_bucket(uint8_t role)
{
    if (role == 1u) {
        return 0; /* Pitcher */
    }
    if (role == 2u) {
        return 1; /* Catcher */
    }
    if (role >= 3u && role <= 6u) {
        return 2; /* Infielder */
    }
    if (role >= 7u && role <= 9u) {
        return 3; /* Outfielder */
    }
    return 4;
}

int kbo_fa_team_role_count(uint32_t team_id, int role_bucket)
{
    if (team_id == 0u || role_bucket < 0) {
        return 0;
    }
    if (role_bucket >= KBO_FA_ROLE_BUCKET_COUNT) {
        role_bucket = KBO_FA_ROLE_BUCKET_COUNT - 1;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || !kbo_fa_score_player_vector_readable(player_vector, player_count)) {
        return 0;
    }

    uint32_t slot_index = ((team_id * 2654435761u) ^ ((uint32_t)player_count << 3))
        & (KBO_FA_TEAM_ROLE_COUNT_CACHE_SIZE - 1u);
    KboFaTeamRoleCountCacheEntry* cached = &g_kbo_fa_team_role_count_cache[slot_index];
    if (cached->valid
            && cached->team_id == team_id
            && cached->player_vector == player_vector
            && cached->player_count == player_count) {
        return cached->counts[role_bucket];
    }

    int counts[KBO_FA_ROLE_BUCKET_COUNT] = {0, 0, 0, 0, 0};
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t slot = player_vector + ((uintptr_t)i * sizeof(uintptr_t));
        if (!memory_range_readable((void*)slot, sizeof(uintptr_t))) { break; }
        uintptr_t player_ptr = *(uintptr_t*)slot;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u) {
            continue;
        }
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        if (current_team_id != team_id && active_team_id != team_id) {
            continue;
        }
        int bucket = kbo_fa_role_bucket(player[OOTP27_PLAYER_POSITION_ROLE_OFFSET]);
        if (bucket < 0 || bucket >= KBO_FA_ROLE_BUCKET_COUNT) {
            bucket = KBO_FA_ROLE_BUCKET_COUNT - 1;
        }
        counts[bucket]++;
    }

    cached->valid = 0u;
    cached->team_id = team_id;
    cached->player_vector = player_vector;
    cached->player_count = player_count;
    for (int i = 0; i < KBO_FA_ROLE_BUCKET_COUNT; i++) {
        cached->counts[i] = counts[i];
    }
    cached->valid = 1u;
    return counts[role_bucket];
}

int32_t kbo_fa_protection_candidate_score(
    uint8_t* player,
    uint32_t signing_team_id,
    char* reason,
    size_t reason_size)
{
    if (reason != NULL && reason_size > 0) {
        reason[0] = '\0';
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return -2147483647;
    }

    int32_t base = kbo_foreign_waiver_value_score(player);
    int32_t salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint8_t role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
    int role_bucket = kbo_fa_role_bucket(role);
    int team_role_count = kbo_fa_team_role_count(signing_team_id, role_bucket);
    const KboFaCompensationProtectionPolicy* policy = kbo_fa_compensation_protection_policy();

    int32_t score = base;
    const char* age_tag = "prime";
    if ((int32_t)age <= policy->candidate_prospect_age_max) {
        score += policy->candidate_prospect_bonus;
        age_tag = "prospect";
    } else if ((int32_t)age <= policy->candidate_young_age_max) {
        score += policy->candidate_young_bonus;
        age_tag = "young";
    } else if ((int32_t)age <= policy->candidate_prime_age_max) {
        score += policy->candidate_prime_bonus;
        age_tag = "prime";
    } else if ((int32_t)age >= policy->candidate_old_low_overall_age_min
            && overall <= policy->candidate_old_low_overall_max) {
        score -= policy->candidate_old_low_overall_penalty;
        age_tag = "old_low_ovr";
    } else if ((int32_t)age >= policy->candidate_aging_age_min) {
        score -= policy->candidate_aging_penalty;
        age_tag = "aging";
    }

    const char* core_tag = "depth";
    if (overall >= policy->candidate_core_value_min || career >= policy->candidate_core_value_min) {
        score += policy->candidate_core_bonus;
        core_tag = "core";
    } else if (overall >= policy->candidate_regular_value_min || career >= policy->candidate_regular_value_min) {
        score += policy->candidate_regular_bonus;
        core_tag = "regular";
    }
    if (talent > overall + policy->candidate_upside_margin) {
        score += policy->candidate_upside_bonus;
        core_tag = "upside";
    }

    const char* salary_tag = "salary_ok";
    if (salary <= 0) {
        salary_tag = "unknown_salary";
    } else if (salary <= policy->candidate_cheap_salary_max
            && ((int32_t)age <= policy->candidate_cheap_age_max
                || talent >= overall + policy->candidate_cheap_talent_margin)) {
        score += policy->candidate_cheap_bonus;
        salary_tag = "cheap_control";
    } else if (salary >= policy->candidate_bad_contract_salary_min
            && (int32_t)age >= policy->candidate_bad_contract_age_min
            && overall <= policy->candidate_bad_contract_overall_max) {
        score -= policy->candidate_bad_contract_penalty;
        salary_tag = "bad_contract";
    } else if (salary >= policy->candidate_costly_vet_salary_min
            && (int32_t)age >= policy->candidate_costly_vet_age_min) {
        score -= policy->candidate_costly_vet_penalty;
        salary_tag = "costly_vet";
    } else if (salary >= policy->candidate_paid_core_salary_min
            && overall >= policy->candidate_paid_core_overall_min) {
        score += policy->candidate_paid_core_bonus;
        salary_tag = "paid_core";
    }

    const char* scarcity_tag = "normal_role";
    if ((role_bucket == 1 && team_role_count <= policy->candidate_scarce_catcher_max)
            || (role_bucket == 2 && team_role_count <= policy->candidate_scarce_infielder_max)
            || (role_bucket == 3 && team_role_count <= policy->candidate_scarce_outfielder_max)
            || (role_bucket == 0 && team_role_count <= policy->candidate_scarce_pitcher_max)) {
        score += policy->candidate_scarce_bonus;
        scarcity_tag = "scarce_role";
    } else if ((role_bucket == 1 && team_role_count >= policy->candidate_deep_catcher_min)
            || (role_bucket == 2 && team_role_count >= policy->candidate_deep_infielder_min)
            || (role_bucket == 3 && team_role_count >= policy->candidate_deep_outfielder_min)
            || (role_bucket == 0 && team_role_count >= policy->candidate_deep_pitcher_min)) {
        score -= policy->candidate_deep_penalty;
        scarcity_tag = "deep_role";
    }

    if (reason != NULL && reason_size > 0) {
        snprintf(
            reason,
            reason_size,
            "%s/%s/%s/%s/base%d",
            age_tag,
            core_tag,
            salary_tag,
            scarcity_tag,
            base);
    }
    return score;
}
