#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "secondary_draft_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/logging/core_log.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../player_team_history/player_team_seasons.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"

static int kbo_secondary_draft_team_index_by_org(
    const KboSecondaryDraftTeam* teams,
    int team_count,
    uint32_t team_id)
{
    if (team_id == 0u) {
        return -1;
    }
    int direct = kbo_secondary_draft_team_index_by_id(teams, team_count, team_id);
    if (direct >= 0) {
        return direct;
    }
    uint32_t org_team_id = kbo_org_team_id_for_team_id(team_id);
    return kbo_secondary_draft_team_index_by_id(teams, team_count, org_team_id);
}

int kbo_secondary_draft_owner_index_for_player(
    uint8_t* player,
    const KboSecondaryDraftTeam* teams,
    int team_count)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return -1;
    }
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    int index = kbo_secondary_draft_team_index_by_org(teams, team_count, active_team_id);
    if (index >= 0) {
        return index;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    index = kbo_secondary_draft_team_index_by_org(teams, team_count, current_team_id);
    if (index >= 0) {
        return index;
    }

    uint32_t default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    return kbo_secondary_draft_team_index_by_org(teams, team_count, default_team_id);
}

static uintptr_t* kbo_secondary_draft_copy_player_vector_snapshot(int32_t* out_count, const char** out_failure)
{
    if (out_count != NULL) {
        *out_count = 0;
    }
    if (out_failure != NULL) {
        *out_failure = "unknown";
    }

    uintptr_t player_vector = 0u;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        if (out_failure != NULL) { *out_failure = "vector_unavailable"; }
        return NULL;
    }
    if (player_vector == 0u || player_count <= 0 || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT) {
        if (out_failure != NULL) { *out_failure = "invalid_vector"; }
        return NULL;
    }
    if ((SIZE_T)player_count > ((SIZE_T)-1 / sizeof(uintptr_t))) {
        if (out_failure != NULL) { *out_failure = "count_overflow"; }
        return NULL;
    }

    SIZE_T bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, bytes)) {
        if (out_failure != NULL) { *out_failure = "unreadable_vector"; }
        return NULL;
    }

    uintptr_t* snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, bytes);
    if (snapshot == NULL) {
        if (out_failure != NULL) { *out_failure = "alloc_failed"; }
        return NULL;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)player_vector, snapshot, bytes, &bytes_read)
            || bytes_read != bytes) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        if (out_failure != NULL) { *out_failure = "copy_failed"; }
        return NULL;
    }

    if (out_count != NULL) {
        *out_count = player_count;
    }
    if (out_failure != NULL) {
        *out_failure = NULL;
    }
    return snapshot;
}

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

    int32_t salary = 0;
    if (memory_range_readable(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET, sizeof(int32_t))) {
        salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    }
    if (salary > 0) {
        score -= salary / 10000000;
    }
    return score;
}

static int kbo_secondary_draft_player_auto_protected_by_tenure(
    uint8_t* player,
    int* out_total_seasons,
    int* out_known)
{
    if (out_total_seasons != NULL) {
        *out_total_seasons = 0;
    }
    if (out_known != NULL) {
        *out_known = 0;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 1;
    }

    int total_seasons = 0;
    if (kbo_player_team_total_seasons_for_player(player, &total_seasons)) {
        if (out_total_seasons != NULL) {
            *out_total_seasons = total_seasons;
        }
        if (out_known != NULL) {
            *out_known = 1;
        }
        return total_seasons <= 3;
    }

    uint16_t service_days = kbo_secondary_draft_read_player_u16(player, OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET);
    uint16_t age = kbo_secondary_draft_read_player_u16(player, OOTP27_PLAYER_AGE_OFFSET);
    uint32_t protected_service_days = KBO_SECONDARY_DRAFT_SERVICE_DAYS_PER_SEASON * 3u;
    if (service_days > 0u && service_days <= protected_service_days && age <= 25u) {
        return 1;
    }
    if (service_days == 0u && age >= 18u && age <= 23u) {
        return 1;
    }
    return 0;
}

int kbo_secondary_draft_player_status_ok(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    if (kbo_player_is_retired(player)
            || kbo_player_is_foreign_for_kbo_rights(player)
            || kbo_player_is_draft_pool_candidate(player)
            || !kbo_player_has_nonzero_evaluation(player)) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) == 0u
            || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u) {
        return 0;
    }
    if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u
            || player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) != 0u) {
        return 0;
    }
    return 1;
}

int kbo_secondary_draft_collect_candidates(
    const KboSecondaryDraftTeam* teams,
    int team_count,
    KboSecondaryDraftCandidate* candidates,
    int max_candidates)
{
    if (teams == NULL || team_count <= 0 || candidates == NULL || max_candidates <= 0) {
        return 0;
    }

    int32_t player_count = 0;
    const char* snapshot_failure = NULL;
    uintptr_t* snapshot = kbo_secondary_draft_copy_player_vector_snapshot(&player_count, &snapshot_failure);
    if (snapshot == NULL) {
        kbo_log_runtimef(
            "KBO secondary draft skipped reason=player_vector_snapshot_failed detail=%s",
            snapshot_failure != NULL ? snapshot_failure : "unknown");
        return 0;
    }

    int count = 0;
    for (int32_t i = 0; i < player_count && count < max_candidates; i++) {
        uintptr_t player_ptr = snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_secondary_draft_player_status_ok(player)) {
            continue;
        }

        int owner_index = kbo_secondary_draft_owner_index_for_player(player, teams, team_count);
        if (owner_index < 0) {
            continue;
        }

        KboSecondaryDraftCandidate* c = &candidates[count];
        memset(c, 0, sizeof(*c));
        c->player_ptr = player_ptr;
        c->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        c->owner_index = owner_index;
        c->owner_team_id = teams[owner_index].team_id;
        c->value_score = kbo_secondary_draft_player_value_score(player);
        c->age = kbo_secondary_draft_read_player_u16(player, OOTP27_PLAYER_AGE_OFFSET);
        c->service_days = kbo_secondary_draft_read_player_u16(player, OOTP27_PLAYER_SERVICE_TIME_DAYS_OFFSET);
        c->position_role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
        c->auto_protected = (uint8_t)kbo_secondary_draft_player_auto_protected_by_tenure(
            player,
            &c->total_seasons,
            &c->total_seasons_known);
        c->protected_player = c->auto_protected;
        kbo_copy_player_display_name(player, c->player_name, sizeof(c->player_name));
        if (c->player_name[0] == '\0') {
            snprintf(c->player_name, sizeof(c->player_name), "Player #%u", c->player_id);
        }
        count++;
    }
    HeapFree(GetProcessHeap(), 0, snapshot);
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
    for (int i = 0; i < candidate_count; i++) {
        if (candidates[i].protected_player) {
            protected_count++;
        }
    }

    for (int team_index = 0; team_index < team_count; team_index++) {
        uint32_t team_id = 0u;
        for (int i = 0; i < candidate_count; i++) {
            if (candidates[i].owner_index == team_index && candidates[i].owner_team_id != 0u) {
                team_id = candidates[i].owner_team_id;
                break;
            }
        }
        int submitted_count = 0;
        int submitted = season != 0u
            && team_id != 0u
            && kbo_secondary_draft_sql_team_submitted(season, team_id, &submitted_count);
        if (submitted) {
            for (int i = 0; i < candidate_count; i++) {
                KboSecondaryDraftCandidate* c = &candidates[i];
                if (c->owner_index != team_index || c->protected_player || c->selected) {
                    continue;
                }
                if (kbo_secondary_draft_sql_player_protected(season, team_id, c->player_id)) {
                    c->protected_player = 1u;
                    protected_count++;
                }
            }
            continue;
        }
        for (int protected_slot = 0; protected_slot < KBO_SECONDARY_DRAFT_PROTECTED_COUNT; protected_slot++) {
            int best_index = -1;
            int32_t best_score = INT_MIN;
            for (int i = 0; i < candidate_count; i++) {
                KboSecondaryDraftCandidate* c = &candidates[i];
                if (c->owner_index != team_index || c->protected_player || c->selected) {
                    continue;
                }
                if (best_index < 0
                        || c->value_score > best_score
                        || (c->value_score == best_score && c->player_id < candidates[best_index].player_id)) {
                    best_index = i;
                    best_score = c->value_score;
                }
            }
            if (best_index < 0) {
                break;
            }
            candidates[best_index].protected_player = 1u;
            protected_count++;
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
                || teams[c->owner_index].loss_count >= KBO_SECONDARY_DRAFT_SOURCE_LOSS_LIMIT) {
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
