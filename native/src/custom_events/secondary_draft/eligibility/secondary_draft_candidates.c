#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../secondary_draft_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/profiling/profiler.h"
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

typedef struct KboSecondaryDraftFaLookupEntry {
    uint32_t player_id;
} KboSecondaryDraftFaLookupEntry;

static int kbo_secondary_draft_current_year_fa_slow(uint32_t player_id, uint32_t season)
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

static int kbo_secondary_draft_fa_lookup_cmp(const void* left, const void* right)
{
    const KboSecondaryDraftFaLookupEntry* a = (const KboSecondaryDraftFaLookupEntry*)left;
    const KboSecondaryDraftFaLookupEntry* b = (const KboSecondaryDraftFaLookupEntry*)right;
    if (a->player_id == b->player_id) {
        return 0;
    }
    return a->player_id < b->player_id ? -1 : 1;
}

static int kbo_secondary_draft_current_year_fa_lookup(
    const KboSecondaryDraftFaLookupEntry* entries,
    int count,
    uint32_t player_id)
{
    if (entries == NULL || count <= 0 || player_id == 0u) {
        return 0;
    }
    int lo = 0;
    int hi = count - 1;
    while (lo <= hi) {
        int mid = lo + ((hi - lo) / 2);
        uint32_t candidate_id = entries[mid].player_id;
        if (candidate_id == player_id) {
            return 1;
        }
        if (candidate_id < player_id) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return 0;
}

static int kbo_secondary_draft_build_current_year_fa_lookup(
    uint32_t season,
    KboSecondaryDraftFaLookupEntry* entries,
    int max_entries)
{
    if (season == 0u || entries == NULL || max_entries <= 0) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_secondary_draft_fa_decisions_load);
    KboFaDeclarationDecision* decisions = (KboFaDeclarationDecision*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_DECLARATION_REPORT_MAX * sizeof(KboFaDeclarationDecision));
    int decision_count = decisions != NULL
        ? kbo_fa_declaration_load_season_decisions(
            season,
            decisions,
            KBO_FA_DECLARATION_REPORT_MAX)
        : -1;
    KBO_PROFILE_END(profile_secondary_draft_fa_decisions_load, "secondary_draft.collect.fa_decisions_load");
    if (decisions == NULL) {
        return -1;
    }

    KBO_PROFILE_BEGIN(profile_secondary_draft_fa_lookup_build);
    int lookup_count = 0;
    for (int i = 0; i < decision_count && lookup_count < max_entries; i++) {
        const KboFaDeclarationDecision* decision = &decisions[i];
        if (decision->season != season || decision->player_id == 0u || decision->declared == 0u) {
            continue;
        }
        entries[lookup_count].player_id = decision->player_id;
        lookup_count++;
    }
    if (lookup_count > 1) {
        qsort(entries, (size_t)lookup_count, sizeof(entries[0]), kbo_secondary_draft_fa_lookup_cmp);
    }
    KBO_PROFILE_END(profile_secondary_draft_fa_lookup_build, "secondary_draft.collect.fa_lookup_build");

    HeapFree(GetProcessHeap(), 0, decisions);
    return lookup_count;
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

    KBO_PROFILE_BEGIN(profile_secondary_draft_collect_total);
    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)
            || player_vector == 0u
            || player_count <= 0
            || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT
            || !memory_range_readable((void*)player_vector, (SIZE_T)player_count * sizeof(uintptr_t))) {
        kbo_log_runtime_line("KBO secondary draft skipped reason=player_vector_unavailable");
        KBO_PROFILE_END(profile_secondary_draft_collect_total, "secondary_draft.collect.total");
        return 0;
    }

    KboSecondaryDraftTeamOwnerMapEntry owner_map[KBO_SECONDARY_DRAFT_TEAM_OWNER_MAP_MAX];
    KBO_PROFILE_BEGIN(profile_secondary_draft_owner_map);
    int owner_map_count = kbo_secondary_draft_build_team_owner_map(
        teams,
        team_count,
        owner_map,
        KBO_SECONDARY_DRAFT_TEAM_OWNER_MAP_MAX);
    KBO_PROFILE_END(profile_secondary_draft_owner_map, "secondary_draft.collect.owner_map");

    KboSecondaryDraftFaLookupEntry* fa_lookup = (KboSecondaryDraftFaLookupEntry*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_DECLARATION_REPORT_MAX * sizeof(KboSecondaryDraftFaLookupEntry));
    int fa_lookup_count = fa_lookup != NULL
        ? kbo_secondary_draft_build_current_year_fa_lookup(
            season,
            fa_lookup,
            KBO_FA_DECLARATION_REPORT_MAX)
        : -1;

    int count = 0;
    KBO_PROFILE_BEGIN(profile_secondary_draft_player_loop);
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
        input.current_year_fa = fa_lookup_count >= 0
            ? kbo_secondary_draft_current_year_fa_lookup(fa_lookup, fa_lookup_count, player_id)
            : kbo_secondary_draft_current_year_fa_slow(player_id, season);
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
    KBO_PROFILE_END(profile_secondary_draft_player_loop, "secondary_draft.collect.player_loop");
    if (fa_lookup != NULL) {
        HeapFree(GetProcessHeap(), 0, fa_lookup);
    }
    KBO_PROFILE_END(profile_secondary_draft_collect_total, "secondary_draft.collect.total");
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
