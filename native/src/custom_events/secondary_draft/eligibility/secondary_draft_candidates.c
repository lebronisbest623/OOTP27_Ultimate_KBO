#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../secondary_draft_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../fa_declaration/fa_declaration.h"
#include "../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_name_cache.h"

static uint16_t kbo_secondary_draft_read_player_u16(uint8_t* player, uint32_t offset)
{
    if (player == NULL || offset + sizeof(uint16_t) > OOTP27_PLAYER_SCAN_BYTES
            || !memory_range_readable(player + offset, sizeof(uint16_t))) {
        return 0u;
    }
    return *(uint16_t*)(player + offset);
}

static int32_t kbo_secondary_draft_player_value_score(uint8_t* player)
{
    int32_t overall = (int32_t)kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = (int32_t)kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = (int32_t)kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = (int32_t)kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    int32_t score = talent * 6 + overall * 4 + ratings * 2 + career;
    uint16_t age = kbo_secondary_draft_read_player_u16(player, OOTP27_PLAYER_AGE_OFFSET);
    if (age >= 18u && age <= 24u) {
        score += 300;
    } else if (age >= 25u && age <= 28u) {
        score += 150;
    } else if (age > 32u) {
        score -= (int32_t)(age - 32u) * 60;
    }
    int32_t salary = memory_range_readable(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET, sizeof(int32_t))
        ? *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET)
        : 0;
    if (salary > 0) {
        score -= salary / 10000000;
    }
    return score;
}

static int kbo_secondary_draft_current_year_fa(uint32_t player_id, uint32_t season)
{
    if (player_id == 0u || season == 0u) {
        return 0;
    }
    KboFaDeclarationDecision decision;
    memset(&decision, 0, sizeof(decision));
    return kbo_fa_declaration_find_latest_decision(player_id, season, &decision)
        && decision.season == season
        && decision.declared != 0u;
}

int kbo_secondary_draft_collect_candidates(
    uint32_t season,
    const KboSecondaryDraftTeam* teams,
    int team_count,
    KboSecondaryDraftCandidate* candidates,
    int max_candidates)
{
    if (teams == NULL || team_count <= 0 || candidates == NULL || max_candidates <= 0) {
        return 0;
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || player_vector == 0u
            || player_count <= 0
            || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT
            || !memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        kbo_log_runtime_line("KBO secondary draft skipped reason=player_vector_unavailable");
        return 0;
    }

    KboSecondaryDraftTeamOwnerMapEntry owner_map[KBO_SECONDARY_DRAFT_TEAM_OWNER_MAP_MAX];
    int owner_map_count = kbo_secondary_draft_build_team_owner_map(
        teams,
        team_count,
        owner_map,
        KBO_SECONDARY_DRAFT_TEAM_OWNER_MAP_MAX);

    int count = 0;
    for (int32_t i = 0; i < player_count && count < max_candidates; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t owner_team_id = 0u;
        uint32_t service_team_id = 0u;
        int military_reserved = 0;
        int non_military_loan = 0;
        int owner_index = kbo_secondary_draft_owner_index_for_player_from_map(
            player,
            owner_map,
            owner_map_count,
            &owner_team_id,
            &service_team_id,
            &military_reserved,
            &non_military_loan);
        if (owner_index < 0) {
            continue;
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint16_t service_days = kbo_secondary_draft_read_player_u16(player, OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET);
        int total_seasons = (int)(service_days / KBO_SECONDARY_DRAFT_SERVICE_DAYS_PER_SEASON);
        int military_history = military_reserved || player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u;
        KboSecondaryDraftEligibilityInput input;
        memset(&input, 0, sizeof(input));
        input.player_id = player_id;
        input.owner_team_id = teams[owner_index].team_id;
        input.age = kbo_secondary_draft_read_player_u16(player, OOTP27_PLAYER_AGE_OFFSET);
        input.service_days = service_days;
        input.total_seasons = total_seasons;
        input.total_seasons_known = service_days > 0u;
        input.foreign_player = kbo_player_is_foreign_for_kbo_rights(player);
        input.retired = kbo_player_is_retired(player);
        input.dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u;
        input.draft_pool = kbo_player_is_draft_pool_candidate(player);
        input.has_evaluation = kbo_player_has_nonzero_evaluation(player);
        input.current_year_fa = kbo_secondary_draft_current_year_fa(player_id, season);
        input.military_reserved = military_reserved;
        input.military_history = military_history;
        input.non_military_loan = non_military_loan;

        KboSecondaryDraftEligibilityDecision decision;
        if (!kbo_secondary_draft_evaluate_eligibility(&input, &decision)) {
            continue;
        }

        KboSecondaryDraftCandidate* c = &candidates[count++];
        memset(c, 0, sizeof(*c));
        c->player_ptr = player_ptr;
        c->player_id = player_id;
        c->owner_index = owner_index;
        c->owner_team_id = teams[owner_index].team_id;
        c->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        c->service_team_id = service_team_id;
        c->value_score = kbo_secondary_draft_player_value_score(player);
        c->age = input.age;
        c->service_days = service_days;
        c->total_seasons = decision.inferred_total_seasons;
        c->total_seasons_known = input.total_seasons_known;
        c->position_role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
        c->military_reserved = military_reserved ? 1u : 0u;
        snprintf(c->reason, sizeof(c->reason), "%s", decision.reason);
        kbo_copy_player_display_name(player, c->player_name, sizeof(c->player_name));
        if (c->player_name[0] == '\0') {
            snprintf(c->player_name, sizeof(c->player_name), "Player #%u", c->player_id);
        }
        (void)owner_team_id;
    }
    return count;
}

int kbo_secondary_draft_mark_protected_players(
    KboSecondaryDraftCandidate* candidates,
    int candidate_count,
    int team_count,
    uint32_t season)
{
    if (candidates == NULL || candidate_count <= 0 || team_count <= 0) {
        return 0;
    }
    int protected_count = 0;
    for (int team_index = 0; team_index < team_count; team_index++) {
        uint32_t team_id = 0u;
        for (int i = 0; i < candidate_count; i++) {
            if (candidates[i].owner_index == team_index) {
                team_id = candidates[i].owner_team_id;
                break;
            }
        }
        if (team_id == 0u) {
            continue;
        }
        uint32_t protected_ids[KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT] = {0};
        int protected_ids_count = kbo_secondary_draft_sql_load_protected_player_ids(
            season,
            team_id,
            protected_ids,
            KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT);
        for (int i = 0; i < candidate_count; i++) {
            KboSecondaryDraftCandidate* c = &candidates[i];
            if (c->owner_index != team_index || c->protected_player || c->selected) {
                continue;
            }
            if (kbo_secondary_draft_id_list_contains(protected_ids, protected_ids_count, c->player_id)) {
                c->protected_player = 1u;
                protected_count++;
            }
        }
    }
    return protected_count;
}

static int64_t kbo_secondary_draft_selection_score(const KboSecondaryDraftCandidate* candidate)
{
    if (candidate == NULL) {
        return INT64_MIN;
    }
    int64_t score = (int64_t)candidate->value_score * 1000ll;
    if (candidate->age >= 18u && candidate->age <= 27u) {
        score += 250ll;
    }
    if (candidate->total_seasons_known && candidate->total_seasons >= 4 && candidate->total_seasons <= 6) {
        score += 150ll;
    }
    score -= (int64_t)(candidate->player_id % 997u);
    return score;
}

int kbo_secondary_draft_best_candidate_for_team(
    KboSecondaryDraftCandidate* candidates,
    int candidate_count,
    const KboSecondaryDraftTeam* teams,
    int drafting_team_index)
{
    if (candidates == NULL || teams == NULL || drafting_team_index < 0) {
        return -1;
    }
    int best_index = -1;
    int64_t best_score = INT64_MIN;
    for (int i = 0; i < candidate_count; i++) {
        KboSecondaryDraftCandidate* c = &candidates[i];
        if (c->selected
                || c->protected_player
                || c->owner_index == drafting_team_index
                || c->owner_index < 0
                || !kbo_secondary_draft_source_loss_allows(teams[c->owner_index].loss_count)) {
            continue;
        }
        int64_t score = kbo_secondary_draft_selection_score(c);
        if (best_index < 0 || score > best_score) {
            best_index = i;
            best_score = score;
        }
    }
    return best_index;
}
