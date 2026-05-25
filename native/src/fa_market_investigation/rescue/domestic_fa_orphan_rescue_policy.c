#include "domestic_fa_orphan_rescue_policy.h"

#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"

static int kbo_domestic_fa_orphan_rescue_grade_is_quality(const char* grade)
{
    return grade != NULL
        && (strcmp(grade, "A") == 0
            || strcmp(grade, "B") == 0
            || strcmp(grade, "C") == 0);
}

static int kbo_domestic_fa_orphan_rescue_case_is_official_or_probable(const char* case_label)
{
    if (case_label == NULL || case_label[0] == '\0') {
        return 0;
    }
    return strcmp(case_label, "KBO_FA_APPROVED") == 0
        || strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0
        || strcmp(case_label, "KBO_FA_CARRYOVER_UNSIGNED") == 0;
}

int kbo_domestic_fa_orphan_rescue_grade_priority(const char* grade)
{
    if (grade == NULL) {
        return 4;
    }
    if (strcmp(grade, "C") == 0) {
        return 0;
    }
    if (strcmp(grade, "B") == 0) {
        return 1;
    }
    if (strcmp(grade, "A") == 0) {
        return 2;
    }
    return 3;
}

uint32_t kbo_domestic_fa_orphan_rescue_market_days_min(
    const KboFaMarketPolicy* policy)
{
    if (policy == NULL) {
        return 0u;
    }
    int32_t days = policy->orphan_rescue_market_days_min;
    if (days <= 0) {
        days = policy->investigation_market_days_long_min;
    }
    return days > 0 ? (uint32_t)days : 0u;
}

int kbo_domestic_fa_orphan_rescue_candidate_eligible(
    const KboDomesticFaInvestigationCandidate* candidate,
    const KboFaMarketPolicy* policy)
{
    if (candidate == NULL || policy == NULL) {
        return 0;
    }
    const KboFaMarketClassification* row = &candidate->row;
    if (!kbo_domestic_fa_orphan_rescue_case_is_official_or_probable(row->case_label)) {
        return 0;
    }
    if (!kbo_domestic_fa_orphan_rescue_grade_is_quality(row->grade)
            && candidate->value_score < policy->investigation_quality_value_score_min) {
        return 0;
    }
    if (candidate->market_days < kbo_domestic_fa_orphan_rescue_market_days_min(policy)) {
        return 0;
    }
    if (row->fa_demand >= policy->investigation_very_high_demand_min) {
        return 0;
    }
    return row->player_id != 0u
        && row->nation_id == OOTP27_KBO_KOREA_NATION_ID
        && !row->foreign_player
        && row->current_team_id == 0u
        && row->active_team_id == 0u
        && row->retired_flag == 0u;
}

int kbo_domestic_fa_orphan_rescue_candidate_original_team_fit(
    const KboDomesticFaOrphanRescueCachedCandidate* candidate,
    uint32_t requester_team_id)
{
    return candidate != NULL
        && requester_team_id != 0u
        && candidate->original_team_id != 0u
        && candidate->original_team_id == requester_team_id;
}

int kbo_domestic_fa_orphan_rescue_candidate_team_allowed(
    const KboDomesticFaOrphanRescueCachedCandidate* candidate,
    uint32_t requester_team_id,
    const KboFaMarketPolicy* policy)
{
    if (candidate == NULL || requester_team_id == 0u) {
        return 0;
    }
    if (kbo_domestic_fa_orphan_rescue_candidate_original_team_fit(candidate, requester_team_id)) {
        return 1;
    }

    uint32_t cross_team_days_min = kbo_domestic_fa_orphan_rescue_market_days_min(policy);
    if (policy != NULL && policy->investigation_market_days_very_long_min > 0) {
        uint32_t very_long = (uint32_t)policy->investigation_market_days_very_long_min;
        if (very_long > cross_team_days_min) {
            cross_team_days_min = very_long;
        }
    }
    return candidate->market_days >= cross_team_days_min;
}

int kbo_domestic_fa_orphan_rescue_compare_cached_desc(const void* lhs, const void* rhs)
{
    const KboDomesticFaOrphanRescueCachedCandidate* a =
        (const KboDomesticFaOrphanRescueCachedCandidate*)lhs;
    const KboDomesticFaOrphanRescueCachedCandidate* b =
        (const KboDomesticFaOrphanRescueCachedCandidate*)rhs;
    int grade_a = kbo_domestic_fa_orphan_rescue_grade_priority(a->grade);
    int grade_b = kbo_domestic_fa_orphan_rescue_grade_priority(b->grade);
    if (grade_a != grade_b) {
        return grade_a < grade_b ? -1 : 1;
    }
    if (a->value_score != b->value_score) {
        return a->value_score > b->value_score ? -1 : 1;
    }
    if (a->fa_demand != b->fa_demand) {
        return a->fa_demand < b->fa_demand ? -1 : 1;
    }
    if (a->market_days != b->market_days) {
        return a->market_days > b->market_days ? -1 : 1;
    }
    if (a->age != b->age) {
        return a->age < b->age ? -1 : 1;
    }
    if (a->player_id == b->player_id) {
        return 0;
    }
    return a->player_id < b->player_id ? -1 : 1;
}

int kbo_domestic_fa_orphan_rescue_team_start_index(
    uint32_t requester_team_id,
    int candidate_count)
{
    if (requester_team_id == 0u || candidate_count <= 0) {
        return 0;
    }
    return (int)(((requester_team_id - 1u) * KBO_DOMESTIC_FA_ORPHAN_RESCUE_FORCE_PER_CALL_MAX)
        % (uint32_t)candidate_count);
}
