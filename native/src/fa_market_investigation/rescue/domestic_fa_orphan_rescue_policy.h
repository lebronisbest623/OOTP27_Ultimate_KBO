#ifndef KBOFIX_SRC_FA_MARKET_INVESTIGATION_RESCUE_DOMESTIC_FA_ORPHAN_RESCUE_POLICY_H_
#define KBOFIX_SRC_FA_MARKET_INVESTIGATION_RESCUE_DOMESTIC_FA_ORPHAN_RESCUE_POLICY_H_

#include <stdint.h>

#include "../thread/domestic_fa_market_investigation_scan.h"
#include "../../fa_market_classification/policy/fa_market_policy.h"

#define KBO_DOMESTIC_FA_ORPHAN_RESCUE_CACHE_MAX 32
#define KBO_DOMESTIC_FA_ORPHAN_RESCUE_FORCE_PER_CALL_MAX 4

typedef struct KboDomesticFaOrphanRescueCachedCandidate {
    uint32_t player_id;
    uint32_t today;
    uint32_t market_days;
    uint32_t original_team_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t nation_id;
    uint32_t fa_filing_season;
    uint16_t age;
    int32_t value_score;
    int32_t fa_demand;
    int32_t fa_grade_salary;
    char grade[12];
    char case_label[48];
} KboDomesticFaOrphanRescueCachedCandidate;

int kbo_domestic_fa_orphan_rescue_grade_priority(const char* grade);
uint32_t kbo_domestic_fa_orphan_rescue_market_days_min(
    const KboFaMarketPolicy* policy);
int kbo_domestic_fa_orphan_rescue_candidate_eligible(
    const KboDomesticFaInvestigationCandidate* candidate,
    const KboFaMarketPolicy* policy);
int kbo_domestic_fa_orphan_rescue_candidate_original_team_fit(
    const KboDomesticFaOrphanRescueCachedCandidate* candidate,
    uint32_t requester_team_id);
int kbo_domestic_fa_orphan_rescue_candidate_team_allowed(
    const KboDomesticFaOrphanRescueCachedCandidate* candidate,
    uint32_t requester_team_id,
    const KboFaMarketPolicy* policy);
int kbo_domestic_fa_orphan_rescue_compare_cached_desc(const void* lhs, const void* rhs);
int kbo_domestic_fa_orphan_rescue_team_start_index(
    uint32_t requester_team_id,
    int candidate_count);

#endif
