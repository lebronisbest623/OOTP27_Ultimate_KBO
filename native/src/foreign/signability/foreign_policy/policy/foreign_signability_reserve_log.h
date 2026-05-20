#ifndef KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_POLICY_POLICY_FOREIGN_SIGNABILITY_RESERVE_LOG_H_
#define KBOFIX_SRC_FOREIGN_SIGNABILITY_FOREIGN_POLICY_POLICY_FOREIGN_SIGNABILITY_RESERVE_LOG_H_

#include <stdint.h>

void kbo_log_foreign_reserve_holder_visible_generic_signability(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    int original_signability,
    int adjusted,
    uint32_t today,
    uintptr_t caller_rva,
    int32_t score,
    int32_t threshold);
void kbo_log_foreign_reserve_blocked_generic_signability(
    uint8_t* player,
    uint32_t player_id,
    uint32_t holder_team_id,
    int original_signability,
    uint32_t today,
    uintptr_t caller_rva,
    int32_t score,
    int32_t threshold);
void kbo_log_foreign_reserve_blocked_display_signability(
    uint8_t* player,
    uint32_t player_id,
    uint32_t team_id,
    uint32_t holder_team_id,
    int original_signability,
    uint32_t today,
    uintptr_t caller_rva);

#endif
