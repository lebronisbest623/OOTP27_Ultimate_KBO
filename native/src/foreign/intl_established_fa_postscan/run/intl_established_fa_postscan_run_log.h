#ifndef KBOFIX_SRC_FOREIGN_INTL_ESTABLISHED_FA_POSTSCAN_RUN_INTL_ESTABLISHED_FA_POSTSCAN_RUN_LOG_H_
#define KBOFIX_SRC_FOREIGN_INTL_ESTABLISHED_FA_POSTSCAN_RUN_INTL_ESTABLISHED_FA_POSTSCAN_RUN_LOG_H_

#include "../internal/intl_established_fa_postscan_internal.h"

void kbo_intl_established_fa_postscan_log_player_detail(
    const KboIntlEstablishedFaPostscanState* batch,
    int32_t index,
    uint8_t* player,
    uint32_t player_id,
    uint32_t nation_id,
    int is_asian,
    int candidate_ok,
    int has_context,
    const KboIntlEstablishedFaMarketNormalization* market_norm,
    uint8_t original_draft_eligible,
    const char* quality_policy,
    int32_t quality_cap,
    int16_t quality_field_cap,
    int32_t original_score,
    int32_t score,
    int was_quality_adjusted);

#endif
