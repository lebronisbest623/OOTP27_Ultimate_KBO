#ifndef KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_INTERNAL_H_
#define KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_INTERNAL_H_

#include <windows.h>
#include <stdint.h>

void kbo_team_add_note_foreign_assignment_success(
    uint8_t* player,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_loan_team_id);
void kbo_team_add_player_record_fa_compensation_success(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id);
int kbo_team_add_foreign_policy_should_block(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t team_id,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id,
    uint32_t caller_rva);
int kbo_team_add_known_foreign_market_minor_caller(uint32_t caller_rva);
void kbo_team_add_normalize_foreign_retention_contract_success(
    uint32_t caller_rva,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id);
void kbo_team_add_attach_foreign_injury_replacement_success(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    const char* source);
void kbo_log_foreign_team_add_trace(
    uint32_t caller_rva,
    const char* result_label,
    uint8_t result,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id);
uint32_t kbo_team_add_cached_amateur_league_id(uint8_t* team);
int kbo_team_add_amateur_verbose_log_enabled_cached(void);
int kbo_team_add_retry_rejected_targets_enabled_cached(void);
uintptr_t kbo_team_add_prepare_amateur_pre_original_reroute(
    uint32_t caller_rva,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int amateur_generation_call,
    int team_readable,
    int player_plausible,
    uint8_t* team,
    uint8_t* player,
    uint32_t* out_amateur_league_id,
    int* out_amateur_pre_rerouted);
void kbo_team_add_apply_success_side_effects(
    uintptr_t effective_team_ptr,
    uintptr_t player_ptr,
    uint8_t* player,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_loan_team_id,
    uint32_t before_original_team_id);

#endif
