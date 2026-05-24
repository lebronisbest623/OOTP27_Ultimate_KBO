#include "../secondary_draft_internal.h"

#include <limits.h>
#include <stdint.h>

#include "../../../core/logging/core_log.h"

static int kbo_secondary_draft_candidate_auto_protectable_for_team(
    const KboSecondaryDraftCandidate* candidate,
    int team_index)
{
    return candidate != NULL
        && candidate->player_id != 0u
        && candidate->owner_index == team_index
        && !candidate->auto_protected
        && !candidate->protected_player
        && !candidate->selected;
}

static int kbo_secondary_draft_next_auto_protection_candidate(
    KboSecondaryDraftCandidate* candidates,
    int candidate_count,
    int team_index)
{
    int best_index = -1;
    int32_t best_score = INT_MIN;
    for (int i = 0; i < candidate_count; i++) {
        KboSecondaryDraftCandidate* candidate = &candidates[i];
        if (!kbo_secondary_draft_candidate_auto_protectable_for_team(candidate, team_index)) {
            continue;
        }
        if (best_index < 0
                || candidate->value_score > best_score
                || (candidate->value_score == best_score
                    && candidate->player_id < candidates[best_index].player_id)) {
            best_index = i;
            best_score = candidate->value_score;
        }
    }
    return best_index;
}

static void kbo_secondary_draft_unmark_auto_protection_indices(
    KboSecondaryDraftCandidate* candidates,
    const int* indices,
    int count)
{
    if (candidates == NULL || indices == NULL || count <= 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        int index = indices[i];
        if (index >= 0) {
            candidates[index].protected_player = candidates[index].auto_protected;
        }
    }
}

static void kbo_secondary_draft_delete_auto_protection_ids(
    uint32_t season,
    uint32_t team_id,
    const uint32_t* player_ids,
    int count)
{
    if (season == 0u || team_id == 0u || player_ids == NULL || count <= 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        if (player_ids[i] != 0u) {
            (void)kbo_secondary_draft_sql_delete_protected_player(season, team_id, player_ids[i]);
        }
    }
}

static int kbo_secondary_draft_mark_existing_protection_ids(
    KboSecondaryDraftCandidate* candidates,
    int candidate_count,
    int team_index,
    const uint32_t* protected_ids,
    int protected_id_count)
{
    if (candidates == NULL || candidate_count <= 0 || protected_ids == NULL || protected_id_count <= 0) {
        return 0;
    }

    int marked = 0;
    for (int i = 0; i < candidate_count; i++) {
        KboSecondaryDraftCandidate* candidate = &candidates[i];
        if (candidate->owner_index != team_index || candidate->protected_player || candidate->selected) {
            continue;
        }
        if (kbo_secondary_draft_id_list_contains(protected_ids, protected_id_count, candidate->player_id)) {
            candidate->protected_player = 1u;
            marked++;
        }
    }
    return marked;
}

int kbo_secondary_draft_auto_submit_missing_protection_lists(
    uint32_t season,
    const KboSecondaryDraftTeam* teams,
    int team_count,
    KboSecondaryDraftCandidate* candidates,
    int candidate_count,
    const char* source)
{
    if (season == 0u || teams == NULL || team_count <= 0 || candidates == NULL || candidate_count <= 0) {
        return 0;
    }
    if (kbo_secondary_draft_sql_run_exists(season) || kbo_secondary_draft_sql_result_count(season) > 0) {
        return 0;
    }

    const char* write_source = source != NULL ? source : "secondary_draft_auto_protection";
    int submitted_teams = 0;
    int total_written = 0;

    for (int team_index = 0; team_index < team_count; team_index++) {
        const KboSecondaryDraftTeam* team = &teams[team_index];
        if (team->team_id == 0u || kbo_secondary_draft_sql_team_submitted(season, team->team_id, NULL)) {
            continue;
        }

        int saved_count = kbo_secondary_draft_sql_protected_count(season, team->team_id);
        if (saved_count > KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT) {
            continue;
        }
        uint32_t saved_ids[KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT] = {0};
        int saved_id_count = kbo_secondary_draft_sql_load_protected_player_ids(
            season,
            team->team_id,
            saved_ids,
            KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT);
        (void)kbo_secondary_draft_mark_existing_protection_ids(
            candidates,
            candidate_count,
            team_index,
            saved_ids,
            saved_id_count);

        int written_indices[KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT] = {0};
        uint32_t written_ids[KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT] = {0};
        int auto_written = 0;
        int protected_total = saved_count;
        int write_failed = 0;
        for (int slot = protected_total; slot < KBO_SECONDARY_DRAFT_PROTECTED_LIST_LIMIT; slot++) {
            int candidate_index = kbo_secondary_draft_next_auto_protection_candidate(
                candidates,
                candidate_count,
                team_index);
            if (candidate_index < 0) {
                break;
            }

            KboSecondaryDraftCandidate* candidate = &candidates[candidate_index];
            if (!kbo_secondary_draft_sql_write_protected_player(
                    season,
                    team->team_id,
                    candidate->player_id,
                    candidate->player_name,
                    team->name,
                    write_source)) {
                write_failed = 1;
                break;
            }

            candidate->protected_player = 1u;
            written_indices[auto_written] = candidate_index;
            written_ids[auto_written] = candidate->player_id;
            auto_written++;
            protected_total++;
        }

        if (write_failed
                || !kbo_secondary_draft_sql_submit_team(
                    season,
                    team->team_id,
                    team->name,
                    protected_total,
                    write_source)) {
            kbo_secondary_draft_unmark_auto_protection_indices(candidates, written_indices, auto_written);
            kbo_secondary_draft_delete_auto_protection_ids(season, team->team_id, written_ids, auto_written);
            continue;
        }

        submitted_teams++;
        total_written += auto_written;
    }

    if (submitted_teams > 0) {
        kbo_log_runtimef(
            "KBO secondary draft auto-submitted missing protection lists source=%s season=%u teams=%d players=%d",
            write_source,
            season,
            submitted_teams,
            total_written);
    }
    return submitted_teams;
}
