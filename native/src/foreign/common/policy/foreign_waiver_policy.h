#ifndef KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_POLICY_H_
#define KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_POLICY_H_

#include <stdint.h>

#include "foreign_player_policy.h"

#ifndef KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT
#define KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT kbo_custom_foreign_base_effective_limit()
#endif

int kbo_foreign_waiver_ai_enabled(void);
int kbo_custom_foreign_policy_enabled(void);
void kbo_count_team_asian_quota_probe(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count);
uint32_t kbo_effective_foreign_count_with_asian_quota(
    uint32_t asian_count,
    uint32_t non_asian_foreign_count);
void kbo_count_active_foreign_for_asian_quota(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers,
    uint32_t* out_non_asian_hitters,
    uint32_t* out_non_asian_pitchers);
uint8_t kbo_custom_foreign_policy_callup_allows(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t active_count,
    int32_t ootp_limit,
    int check_type);
int kbo_custom_foreign_policy_team_allows_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id);
int kbo_custom_foreign_policy_can_override_original_block(uint8_t* candidate, uint32_t team_id);
int kbo_custom_foreign_policy_team_allows_final_signing(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id);
void kbo_record_recent_custom_foreign_policy_block(
    uint32_t player_id,
    uint32_t requester_team_id,
    uint32_t today);
void kbo_record_custom_foreign_pending_offer(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t today);
void kbo_custom_foreign_count_pending_offers(
    uint32_t team_id,
    uint32_t today,
    uint32_t candidate_id,
    uint32_t* out_asian_pending,
    uint32_t* out_non_asian_pending,
    int* out_candidate_pending);
void kbo_cancel_custom_foreign_pending_offer(uint32_t team_id, uint32_t player_id);
int kbo_custom_foreign_policy_trade_allows(
    uintptr_t trade_ptr,
    int32_t requested_side,
    int* out_blocked_side,
    uint32_t* out_team_id,
    uint32_t* out_incoming_player_id,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit);
void kbo_log_asian_quota_signability_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    int original_signability,
    uintptr_t caller_rva);
void kbo_log_asian_quota_offer_probe(
    uint8_t* player,
    uint32_t player_id,
    int32_t team_id,
    uint8_t original_result,
    int32_t flag);

#endif
