#ifndef KBOFIX_SRC_CUSTOM_EVENTS_SECONDARY_DRAFT_UI_ACTIONS_INTERNAL_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_SECONDARY_DRAFT_UI_ACTIONS_INTERNAL_H_

#include "secondary_draft_internal.h"

int kbo_secondary_draft_candidate_cmp_desc(const void* left, const void* right);
uint32_t kbo_secondary_draft_current_date_or_season_anchor(uint32_t season);
void kbo_secondary_draft_fill_ui_candidate_row(
    const KboSecondaryDraftCandidate* candidate,
    const char* team_name,
    uint32_t season,
    int submitted,
    KboSecondaryDraftCandidateRow* out);
void kbo_secondary_draft_fill_ui_candidate_row_with_status(
    const KboSecondaryDraftCandidate* candidate,
    const char* team_name,
    uint32_t season,
    int submitted,
    int saved_protected,
    int already_drafted,
    KboSecondaryDraftCandidateRow* out);
int kbo_secondary_draft_collect_team_candidates(
    uint32_t team_id,
    KboSecondaryDraftTeam* out_team,
    KboSecondaryDraftCandidate* candidates,
    int max_candidates);

#endif
