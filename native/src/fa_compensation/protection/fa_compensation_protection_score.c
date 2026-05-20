#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"
#include "../../bootstrap/profiling/profiler.h"
#include "candidate_score/fa_compensation_candidate_score.h"
#include "fa_compensation_protection_score.h"
#include "policy/fa_compensation_protection_policy.h"
#include "cache/fa_compensation_protection_cache.h"

static int kbo_fa_player_vector_readable(uintptr_t player_vector, int32_t player_count)
{
    return player_vector != 0 && player_count > 0 && player_count <= KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT
        && (SIZE_T)player_count <= ((SIZE_T)-1 / sizeof(uintptr_t))
        && memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t));
}

static int kbo_fa_protected_candidate_compare_desc(const void* left, const void* right)
{
    const KboFaProtectedCandidate* a = (const KboFaProtectedCandidate*)left;
    const KboFaProtectedCandidate* b = (const KboFaProtectedCandidate*)right;
    if (a->score != b->score) {
        return a->score > b->score ? -1 : 1;
    }
    if (a->age != b->age) {
        return a->age < b->age ? -1 : 1;
    }
    if (a->player_id == b->player_id) {
        return 0;
    }
    return a->player_id < b->player_id ? -1 : 1;
}

static int kbo_fa_compensation_player_is_rookie_auto_protected(
    uint8_t* player,
    const KboFaCompensationRecord* rec);

static void kbo_fa_copy_candidate_player_name(uint8_t* player, KboFaProtectedCandidate* candidate)
{
    if (player == NULL || candidate == NULL) {
        return;
    }
    kbo_copy_player_display_name(player, candidate->player_name, sizeof(candidate->player_name));
    if (candidate->player_name[0] == '\0' || strcmp(candidate->player_name, "Unknown player") == 0) {
        snprintf(candidate->player_name, sizeof(candidate->player_name), "Player #%u", candidate->player_id);
    }
}

static void kbo_fa_fill_protection_candidate(
    uint8_t* player,
    uint32_t assignment_team_id,
    uint32_t scoring_team_id,
    KboFaProtectedCandidate* candidate)
{
    if (player == NULL || candidate == NULL) {
        return;
    }
    candidate->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    candidate->team_id = assignment_team_id;
    candidate->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    candidate->role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
    candidate->score = kbo_fa_protection_candidate_score(player, scoring_team_id, candidate->reason, sizeof(candidate->reason));
    kbo_fa_copy_candidate_player_name(player, candidate);
}

static const char* kbo_fa_base_auto_protected_reason(
    uint8_t* player,
    const KboFaCompensationRecord* rec)
{
    if (player == NULL || rec == NULL) {
        return NULL;
    }
    if (kbo_player_is_foreign_for_kbo_rights(player)) {
        return "foreign";
    }
    if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u) {
        return "military";
    }
    if (kbo_fa_compensation_player_is_rookie_auto_protected(player, rec)) {
        return "rookie_draftee";
    }
    return NULL;
}

int32_t kbo_fa_compensation_player_decision_score(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* candidate,
    char* reason,
    size_t reason_size)
{
    if (reason != NULL && reason_size > 0) {
        reason[0] = '\0';
    }
    if (rec == NULL || candidate == NULL || candidate->player_id == 0u) {
        return -2147483647;
    }

    uint8_t* player = kbo_find_player_by_id(candidate->player_id, NULL, NULL);
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        const KboFaCompensationProtectionPolicy* policy = kbo_fa_compensation_protection_policy();
        return candidate->score - policy->decision_missing_player_penalty;
    }

    int32_t base = kbo_foreign_waiver_value_score(player);
    int32_t salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int role_bucket = kbo_fa_role_bucket(candidate->role);
    int team_role_count = kbo_fa_team_role_count(rec->original_team_id, role_bucket);
    const KboFaCompensationProtectionPolicy* policy = kbo_fa_compensation_protection_policy();

    int32_t score = base;
    const char* age_tag = "prime";
    if ((int32_t)candidate->age <= policy->decision_prospect_age_max) {
        score += policy->decision_prospect_bonus;
        age_tag = "prospect";
    } else if ((int32_t)candidate->age <= policy->decision_young_age_max) {
        score += policy->decision_young_bonus;
        age_tag = "young";
    } else if ((int32_t)candidate->age <= policy->decision_prime_age_max) {
        score += policy->decision_prime_bonus;
        age_tag = "prime";
    } else if ((int32_t)candidate->age >= policy->decision_old_age_min) {
        score -= policy->decision_old_penalty;
        age_tag = "old";
    } else if ((int32_t)candidate->age >= policy->decision_aging_age_min) {
        score -= policy->decision_aging_penalty;
        age_tag = "aging";
    }

    const char* salary_tag = "salary_ok";
    if (salary <= 0) {
        score += policy->decision_unknown_salary_bonus;
        salary_tag = "unknown_salary";
    } else if (salary <= policy->decision_cheap_salary_max) {
        score += policy->decision_cheap_bonus;
        salary_tag = "cheap";
    } else if (salary >= policy->decision_expensive_salary_min) {
        score -= policy->decision_expensive_penalty;
        salary_tag = "expensive";
    } else if (salary >= policy->decision_costly_salary_min) {
        score -= policy->decision_costly_penalty;
        salary_tag = "costly";
    }

    const char* need_tag = "neutral_need";
    if ((role_bucket == 1 && team_role_count < policy->decision_need_catcher_below)
            || (role_bucket == 2 && team_role_count < policy->decision_need_infielder_below)
            || (role_bucket == 3 && team_role_count < policy->decision_need_outfielder_below)
            || (role_bucket == 0 && team_role_count < policy->decision_need_pitcher_below)) {
        score += policy->decision_team_need_bonus;
        need_tag = "team_need";
    } else if ((role_bucket == 1 && team_role_count > policy->decision_surplus_catcher_above)
            || (role_bucket == 2 && team_role_count > policy->decision_surplus_infielder_above)
            || (role_bucket == 3 && team_role_count > policy->decision_surplus_outfielder_above)
            || (role_bucket == 0 && team_role_count > policy->decision_surplus_pitcher_above)) {
        score -= policy->decision_surplus_penalty;
        need_tag = "surplus_role";
    }

    if (talent > overall + policy->decision_upside_margin) {
        score += policy->decision_upside_bonus;
    }

    if (reason != NULL && reason_size > 0) {
        snprintf(
            reason,
            reason_size,
            "%s/%s/%s/base%d",
            age_tag,
            salary_tag,
            need_tag,
            base);
    }
    return score;
}

static int kbo_fa_compensation_player_is_rookie_auto_protected(
    uint8_t* player,
    const KboFaCompensationRecord* rec)
{
    if (player == NULL || rec == NULL) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
        return 0;
    }
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    const KboFaCompensationProtectionPolicy* policy = kbo_fa_compensation_protection_policy();
    if ((int32_t)age > policy->rookie_auto_protected_age_max) {
        return 0;
    }
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    if (draft_league_id != rec->league_id) {
        return 0;
    }
    uint8_t draft_class = player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET];
    uint8_t draft_subtype = player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET];
    uint8_t draft_extra = player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET];
    uint8_t generation_context = player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET];
    return draft_class != 0u || draft_subtype != 0u || draft_extra != 0u || generation_context != 0u;
}

int kbo_build_fa_compensation_protected_candidates(
    const KboFaCompensationRecord* rec,
    KboFaProtectedCandidate* candidates,
    int max_candidates,
    KboFaProtectedCandidate* auto_protected,
    int max_auto_protected)
{
    if (rec == NULL || candidates == NULL || max_candidates <= 0
            || rec->signing_team_id == 0u || rec->protect_count == 0u) {
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_fa_protection_build);
    memset(candidates, 0, (SIZE_T)max_candidates * sizeof(candidates[0]));
    if (auto_protected != NULL && max_auto_protected > 0) {
        memset(auto_protected, 0, (SIZE_T)max_auto_protected * sizeof(auto_protected[0]));
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || !kbo_fa_player_vector_readable(player_vector, player_count)) {
        KBO_PROFILE_END(profile_fa_protection_build, "fa_compensation.protection.build.no_vector");
        return 0;
    }

    int cached_count = kbo_fa_try_materialize_cached_protection_candidates(
        rec,
        player_vector,
        player_count,
        candidates,
        max_candidates,
        auto_protected,
        max_auto_protected);
    if (cached_count >= 0) {
        KBO_PROFILE_END(profile_fa_protection_build, "fa_compensation.protection.build.cache_hit");
        return cached_count;
    }

    KboFaProtectedCandidate base_candidates[KBO_FA_PROTECTION_TEAM_CACHE_CANDIDATE_MAX];
    KboFaProtectedCandidate base_auto_protected[KBO_FA_COMPENSATION_PROTECTED_LIST_MAX];
    memset(base_candidates, 0, sizeof(base_candidates));
    memset(base_auto_protected, 0, sizeof(base_auto_protected));

    int count = 0;
    int auto_count = 0;
    for (int32_t i = 0; i < player_count && count < KBO_FA_PROTECTION_TEAM_CACHE_CANDIDATE_MAX; i++) {
        uintptr_t slot = player_vector + ((uintptr_t)i * sizeof(uintptr_t));
        if (!memory_range_readable((void*)slot, sizeof(uintptr_t))) { break; }
        uintptr_t player_ptr = *(uintptr_t*)slot;
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        if (player_id == 0u || (current_team_id != rec->signing_team_id && active_team_id != rec->signing_team_id)) {
            continue;
        }
        if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
            continue;
        }

        const char* auto_reason = kbo_fa_base_auto_protected_reason(player, rec);
        uint32_t assignment_team_id = current_team_id != 0u ? current_team_id : active_team_id;
        if (auto_reason != NULL) {
            if (auto_count < KBO_FA_COMPENSATION_PROTECTED_LIST_MAX) {
                KboFaProtectedCandidate* automatic = &base_auto_protected[auto_count++];
                kbo_fa_fill_protection_candidate(player, assignment_team_id, rec->signing_team_id, automatic);
                snprintf(automatic->reason, sizeof(automatic->reason), "%s", auto_reason);
            }
            continue;
        }

        KboFaProtectedCandidate* candidate = &base_candidates[count++];
        kbo_fa_fill_protection_candidate(player, assignment_team_id, rec->signing_team_id, candidate);
    }

    qsort(base_candidates, (size_t)count, sizeof(base_candidates[0]), kbo_fa_protected_candidate_compare_desc);
    kbo_fa_store_protection_candidate_cache(
        rec,
        player_vector,
        player_count,
        base_candidates,
        count,
        base_auto_protected,
        auto_count);
    int result = kbo_fa_materialize_protection_candidates(
        rec,
        base_candidates,
        count,
        base_auto_protected,
        auto_count,
        candidates,
        max_candidates,
        auto_protected,
        max_auto_protected);
    KBO_PROFILE_END(profile_fa_protection_build, "fa_compensation.protection.build.scanned");
    return result;
}
