#ifndef NATIVE_SRC_FOREIGN_SIGNABILITY_FOREIGN_SIGNABILITY_C_INTERNAL_H
#define NATIVE_SRC_FOREIGN_SIGNABILITY_FOREIGN_SIGNABILITY_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../../bootstrap/profiling/profiler.h"
#include "../../../../core/core_flags/api/flags_api.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/control/team_human_control.h"
#include "../../../../team/lookup/team_lookup.h"
#include "../../../common/dates/foreign_waiver_date.h"
#include "../../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../../common/policy/foreign_waiver_policy.h"
#include "../../../injury/api/foreign_injury.h"
#include "../../../rights/query/foreign_waiver_rights_query.h"
#include "../../../../military_service/military_service.h"
#include "../../state/foreign_fa_block_state.h"


typedef int (__fastcall *OotpPlayerTeamSignabilityFn)(void* player, int32_t team_id, uint16_t year_hint);
#define KBO_TRADE_CHECK_RESULT_FOREIGN_QUOTA_BLOCK (-9)

typedef struct KboFastBlockCustomPolicyResult {
    uint32_t today;
    uint32_t effective_before;
    uint32_t effective_after;
    uint32_t effective_limit;
    uint32_t injured_player_id;
    uint8_t slot_type;
    uint8_t allowed;
    uint8_t valid;
} KboFastBlockCustomPolicyResult;


void kbo_log_foreign_signability_block_callsite(
    uintptr_t caller_rva,
    uint32_t player_id,
    uint32_t requester_team_id,
    uint32_t holder_team_id,
    int original_signability,
    uint32_t today);
int kbo_fast_block_fa_candidate_before_original(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    const char* context,
    uint32_t* out_player_id);
int kbo_fast_block_fa_candidate_before_original_with_result(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    const char* context,
    uint32_t* out_player_id,
    KboFastBlockCustomPolicyResult* out_custom_policy);
int kbo_no_minor_contract_signability_floor(int signability);
int kbo_enforce_foreign_waiver_signability(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    uint16_t year_hint,
    int original_signability,
    uintptr_t caller_rva);

#endif
