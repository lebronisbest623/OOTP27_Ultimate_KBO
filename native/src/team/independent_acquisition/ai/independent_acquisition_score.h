#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_INDEPENDENT_ACQUISITION_SCORE_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_INDEPENDENT_ACQUISITION_SCORE_H_

#include <stdint.h>

int64_t kbo_independent_acquisition_market_interest_adjustment(int market_interest_count);
int64_t kbo_independent_acquisition_team_need_mix_adjustment(
    uint32_t team_id,
    int pitcher,
    int foreign,
    int asian);

#endif
