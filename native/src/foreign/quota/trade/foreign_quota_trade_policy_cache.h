#ifndef KBOFIX_SRC_FOREIGN_QUOTA_TRADE_FOREIGN_QUOTA_TRADE_POLICY_CACHE_H_
#define KBOFIX_SRC_FOREIGN_QUOTA_TRADE_FOREIGN_QUOTA_TRADE_POLICY_CACHE_H_

#include "../internal/foreign_quota_internal.h"

void kbo_custom_foreign_trade_policy_read_player_ids(
    uintptr_t trade_ptr,
    uint32_t player_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS]);
uint32_t kbo_custom_foreign_trade_policy_player_hash(
    const uint32_t* team_ids,
    const uint32_t player_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS]);
int kbo_custom_foreign_trade_policy_cache_hit(
    uintptr_t trade_ptr,
    int32_t requested_side,
    const uint32_t* team_ids,
    const uint32_t player_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS],
    uint32_t player_hash,
    const uint32_t* org_generations,
    uint32_t today,
    uint64_t injury_replacement_fingerprint,
    int* out_blocked_side,
    uint32_t* out_team_id,
    uint32_t* out_incoming_player_id,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    int* out_allowed);
void kbo_custom_foreign_trade_policy_cache_store(
    uintptr_t trade_ptr,
    int32_t requested_side,
    const uint32_t* team_ids,
    const uint32_t player_ids[KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT][KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS],
    uint32_t player_hash,
    const uint32_t* org_generations,
    uint32_t today,
    uint64_t injury_replacement_fingerprint,
    int allowed,
    int blocked_side,
    uint32_t team_id,
    uint32_t incoming_player_id,
    uint32_t effective_before,
    uint32_t effective_after,
    uint32_t effective_limit);

#endif
