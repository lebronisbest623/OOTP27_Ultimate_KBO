#include "../secondary_draft_internal.h"

#include <limits.h>
#include <stdint.h>

#include "../../../core/logging/core_log.h"

static int kbo_secondary_draft_next_auto_protection_candidate(
    KboSecondaryDraftCandidate* candidates,
    int candidate_count,
    int team_index)
{
    int best_index = -1;
    int32_t best_score = INT_MIN;
    for (int i = 0; i < candidate_count; i++) {
        KboSecondaryDraftCandidate* candidate = &candidates[i];
        if (candidate->player_id == 0u
                || candidate->owner_index != team_index
                || candidate->protected_player
                || candidate->selected) {
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
        for (int i = 0; i < candidate_count; i++) {
            if (candidates[i].owner_index == team_index
                    && kbo_secondary_draft_id_list_contains(saved_ids, saved_id_count, candidates[i].player_id)) {
                candidates[i].protected_player = 1u;
            }
        }

        int protected_total = saved_count;
        int auto_written = 0;
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
                break;
            }
            candidate->protected_player = 1u;
            protected_total++;
            auto_written++;
        }
        if (!kbo_secondary_draft_sql_submit_team(
                season,
                team->team_id,
                team->name,
                protected_total,
                write_source)) {
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
