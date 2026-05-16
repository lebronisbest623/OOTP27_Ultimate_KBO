#include "independent_acquisition_score.h"

int64_t kbo_independent_acquisition_market_interest_adjustment(int market_interest_count)
{
    if (market_interest_count <= 0) {
        return 0;
    }
    if (market_interest_count <= 2) {
        return (int64_t)market_interest_count * 2500ll;
    }
    return 5000ll - (int64_t)(market_interest_count - 2) * 14000ll;
}

int64_t kbo_independent_acquisition_team_need_mix_adjustment(
    uint32_t team_id,
    int pitcher,
    int foreign,
    int asian)
{
    uint32_t team_need_mix = team_id * 747796405u
        ^ (pitcher ? 0x27d4eb2du : 0x165667b1u)
        ^ (asian ? 0x85ebca6bu : 0u)
        ^ (foreign ? 0xc2b2ae35u : 0x9e3779b9u);
    team_need_mix ^= team_need_mix >> 15;
    team_need_mix *= 2246822519u;
    team_need_mix ^= team_need_mix >> 13;
    return (int64_t)(team_need_mix % 9000u);
}
