#ifndef NATIVE_SRC_FOREIGN_QUOTA_FOREIGN_QUOTA_C_INTERNAL_H
#define NATIVE_SRC_FOREIGN_QUOTA_FOREIGN_QUOTA_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../bootstrap/profiling/perf_probe.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/sync/lock.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../injury/api/foreign_injury.h"
#include "../../replacement_seed/api/foreign_replacement_seed.h"
#include "../counts/foreign_quota_counts.h"



#define WIN32_LEAN_AND_MEAN
typedef struct KboCustomForeignPendingOffer {
    uint32_t team_id;
    uint32_t player_id;
    uint32_t date_yyyymmdd;
    uint8_t asian_quota_candidate;
} KboCustomForeignPendingOffer;
enum {
    KBO_CUSTOM_FOREIGN_PENDING_OFFER_MAX = 1024
};
#define KBO_CUSTOM_FOREIGN_TRADE_TEAM_ID_OFFSET       0x08u
#define KBO_CUSTOM_FOREIGN_TRADE_PLAYER_ID_OFFSET     0x10u
#define KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT           2
#define KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS         10
#define KBO_CUSTOM_FOREIGN_TRADE_READABLE_BYTES       (KBO_CUSTOM_FOREIGN_TRADE_PLAYER_ID_OFFSET + (KBO_CUSTOM_FOREIGN_TRADE_SIDE_COUNT * KBO_CUSTOM_FOREIGN_TRADE_PLAYER_SLOTS * sizeof(uint32_t)))

extern KboCustomForeignPendingOffer g_kbo_custom_foreign_pending_offers[KBO_CUSTOM_FOREIGN_PENDING_OFFER_MAX];
extern KboLock g_kbo_custom_foreign_pending_offer_lock;
extern int g_kbo_custom_foreign_pending_offer_count;
extern volatile LONG g_kbo_custom_foreign_pending_offer_generation;

LONG kbo_custom_foreign_pending_offer_generation_for_team(uint32_t team_id);
void kbo_custom_foreign_pending_offer_lock(void);
void kbo_custom_foreign_pending_offer_unlock(void);
int kbo_custom_foreign_pending_offer_is_stale(uint32_t offer_date, uint32_t today);
int kbo_custom_foreign_pending_offer_player_now_in_org(uint32_t team_id, uint32_t player_id);
void kbo_custom_foreign_count_pending_offers(
    uint32_t team_id,
    uint32_t today,
    uint32_t candidate_id,
    uint32_t* out_asian_pending,
    uint32_t* out_non_asian_pending,
    int* out_candidate_pending);
void kbo_record_custom_foreign_pending_offer(uint32_t team_id, uint8_t* candidate, uint32_t today);
void kbo_cancel_custom_foreign_pending_offer(uint32_t team_id, uint32_t player_id);
uint32_t kbo_custom_foreign_policy_extra_slots_for_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id);
int kbo_custom_foreign_policy_can_override_original_block(uint8_t* candidate, uint32_t team_id);
int kbo_custom_foreign_policy_team_allows_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id);
int kbo_custom_foreign_policy_team_allows_final_signing(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id);
uint32_t kbo_custom_foreign_trade_team_id(uintptr_t trade_ptr, int side);
uint32_t kbo_custom_foreign_trade_player_id(uintptr_t trade_ptr, int side, int slot);
int kbo_custom_foreign_policy_team_in_trade_scope(uint32_t team_id);
int kbo_custom_foreign_policy_trade_countable_player(
    uint32_t player_id,
    uint8_t** out_player,
    int* out_asian_quota);
void kbo_custom_foreign_policy_trade_adjust_counts_for_player(
    uint32_t team_id,
    uint32_t player_id,
    int incoming,
    uint32_t* asian_count,
    uint32_t* non_asian_count,
    uint32_t* incoming_foreign_count,
    uint32_t* incoming_asian_count,
    uint32_t* first_incoming_player_id);
uint32_t kbo_custom_foreign_policy_trade_extra_slots(
    uintptr_t trade_ptr,
    int incoming_side,
    uint32_t team_id);
int kbo_custom_foreign_policy_trade_allows(
    uintptr_t trade_ptr,
    int32_t requested_side,
    int* out_blocked_side,
    uint32_t* out_team_id,
    uint32_t* out_incoming_player_id,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit);
int32_t kbo_apply_active_asian_quota_count_exception(
    uintptr_t team_ptr,
    int32_t original_count,
    int pitcher_count);
int32_t kbo_custom_foreign_policy_neutralized_count(
    uintptr_t team_ptr,
    int32_t original_count,
    int pitcher_count);
uint8_t kbo_custom_foreign_policy_callup_allows(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t active_count,
    int32_t ootp_limit,
    int check_type);
uint8_t kbo_callup_foreign_limit_allows_with_asian_quota(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t active_count,
    int32_t limit,
    int check_type);
int kbo_asian_quota_exception_available_for_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_team_foreign,
    uint32_t* out_team_asian,
    uint32_t* out_team_non_asian,
    uint32_t* out_team_effective,
    uint32_t* out_effective_after);
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
