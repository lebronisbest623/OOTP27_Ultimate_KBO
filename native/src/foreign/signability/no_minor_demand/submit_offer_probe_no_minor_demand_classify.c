#include "submit_offer_probe_no_minor_demand_internal.h"

static int kbo_no_minor_scan_has_nonzero_evaluation(const uint8_t* scan)
{
    if (scan == NULL) {
        return 0;
    }

    int16_t overall = *(int16_t*)(scan + OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int16_t talent = *(int16_t*)(scan + OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int16_t ratings = *(int16_t*)(scan + OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int16_t career = *(int16_t*)(scan + OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    return overall > 0 || talent > 0 || ratings > 0 || career > 0;
}

int kbo_no_minor_scan_is_teamless_demand_floor_candidate(const uint8_t* scan, uint32_t league_id)
{
    if (scan == NULL) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(scan + OOTP27_PLAYER_ID_OFFSET);
    uint16_t age = *(uint16_t*)(scan + OOTP27_PLAYER_AGE_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(scan + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    if (!kbo_foreign_policy_player_id_plausible(player_id)
            || !kbo_foreign_policy_market_age_allowed(age)
            || current_team_id != 0u) {
        return 0;
    }
    if (scan[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || scan[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET] != 0u) {
        return 0;
    }

    uint32_t current_league_id = *(uint32_t*)(scan + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t draft_league_id = *(uint32_t*)(scan + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    if (league_id != 0u
            && current_league_id != league_id
            && draft_league_id != league_id
            && !kbo_no_minor_scan_has_nonzero_evaluation(scan)) {
        return 0;
    }

    return 1;
}

int kbo_no_minor_scan_is_foreign_fa_candidate(const uint8_t* scan)
{
    if (scan == NULL) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(scan + OOTP27_PLAYER_ID_OFFSET);
    uint16_t age = *(uint16_t*)(scan + OOTP27_PLAYER_AGE_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(scan + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(scan + OOTP27_PLAYER_NATION_ID_OFFSET);
    int32_t demand = *(int32_t*)(scan + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    if (!kbo_foreign_policy_player_id_plausible(player_id)
            || !kbo_foreign_policy_market_age_allowed(age)
            || current_team_id != 0u) {
        return 0;
    }
    if (scan[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
        return 0;
    }
    if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
        return 0;
    }
    return kbo_foreign_policy_demand_salary_plausible(demand);
}

int kbo_no_minor_scan_should_floor_teamless_demand(const uint8_t* scan, uint32_t league_id)
{
    return kbo_no_minor_scan_is_teamless_demand_floor_candidate(scan, league_id)
        && !kbo_no_minor_scan_is_foreign_fa_candidate(scan);
}
