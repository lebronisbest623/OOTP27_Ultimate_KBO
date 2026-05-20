#ifndef KBOFIX_SRC_FOREIGN_SIGNABILITY_SUBMIT_OFFER_PROBE_SUBMIT_OFFER_PROBE_H_
#define KBOFIX_SRC_FOREIGN_SIGNABILITY_SUBMIT_OFFER_PROBE_SUBMIT_OFFER_PROBE_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../build_verify/build_verify.h"
#include "../../../bootstrap/profiling/profiler.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../injury/api/foreign_injury.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "../api/foreign_signability_salary_floor.h"
#include "../../../military_service/military_service.h"
#include "../state/foreign_fa_block_state.h"
#include "../state/submit_offer_probe_state.h"

#define WIN32_LEAN_AND_MEAN
typedef void (__fastcall *OotpFaSubmitOfferProbeFn)(void* screen);
typedef int (__fastcall *OotpFaOfferScreenCallbackProbeFn)(void* screen, void* sender, uintptr_t callback_id, uintptr_t value);
typedef int (__fastcall *OotpFaContractOfferCallbackProbeFn)(void* offer, void* sender, uintptr_t callback_id, uintptr_t value);
typedef uint8_t (__fastcall *OotpPlayerActionEligibilityFn)(void* action_context, int32_t action_id, uint8_t strict_check);
typedef uint8_t* (__fastcall *OotpLeagueFinancialsLookupFn)(void* global_db, int32_t league_id);
typedef struct KboFinancialSalaryLadderSnapshot {
    uint8_t* financials;
    int32_t values[9];
    LONG active;
} KboFinancialSalaryLadderSnapshot;
typedef struct KboForeignFaDemandRemapRecord {
    uint32_t player_id;
    int32_t original_demand;
    int32_t mapped_demand;
} KboForeignFaDemandRemapRecord;
extern LONG g_kbo_no_minor_contract_demand_floor_enabled;
extern KboFinancialSalaryLadderSnapshot g_kbo_foreign_fa_demand_ladder_snapshot;
extern KboForeignFaDemandRemapRecord g_kbo_foreign_fa_demand_remap_records[512];
extern volatile LONG g_kbo_foreign_fa_demand_remap_record_cursor;
extern volatile LONG g_kbo_foreign_fa_demand_restore_timer_pending;
extern const uint32_t KBO_FINANCIALS_SALARY_LADDER_OFFSETS[9];
extern volatile LONG g_kbo_no_minor_contract_demand_floor_scanner_started;

void kbo_enable_no_minor_contract_demand_floor(void);
int kbo_foreign_fa_demand_remap_already_applied(uint32_t player_id, int32_t demand);
void kbo_foreign_fa_demand_remap_remember(uint32_t player_id, int32_t original_demand, int32_t mapped_demand);
int kbo_foreign_fa_demand_remap_candidate(uint8_t* player);
int32_t kbo_foreign_fa_remap_demand_from_salary_ladder(int32_t demand, uint8_t* financials, int asian_quota);
int32_t kbo_foreign_reserve_demand_floor_for_player(
    uint8_t* player,
    uint32_t today,
    uint32_t* out_holder_team_id,
    int32_t* out_score,
    int* out_index,
    int* out_asian_quota);
int kbo_apply_foreign_reserve_demand_floor(uintptr_t player_ptr, const char* source);
DWORD WINAPI kbo_foreign_fa_demand_restore_timer_thread(void* param);
int kbo_write_i32(uint8_t* address, int32_t value);
void kbo_restore_foreign_fa_demand_salary_ladder(const char* source);
void kbo_schedule_foreign_fa_demand_restore_timer(void);
void kbo_prepare_foreign_fa_offer_demand_baseline(uintptr_t player_ptr, const char* source);
uint8_t* kbo_resolve_current_league_financials(uint32_t* out_league_id);
void kbo_log_financials_salary_baseline_probe(const char* source);
int32_t kbo_no_minor_resolve_current_league_id(void);
int32_t kbo_no_minor_resolve_current_league_minimum_salary(void);
int32_t kbo_no_minor_current_league_minimum_salary(void);
int kbo_no_minor_clamp_player_demand_salary(uintptr_t player_ptr, uintptr_t screen_ptr, const char* source);
int kbo_no_minor_clamp_offer_screen_player_salary(uintptr_t screen_ptr, const char* source);
uintptr_t* kbo_no_minor_copy_player_vector_snapshot(
    uintptr_t player_vector,
    int32_t player_count,
    const char** out_failure_reason);
int kbo_no_minor_player_is_teamless_demand_floor_candidate(uint8_t* player, uint32_t league_id);
int kbo_no_minor_scan_and_floor_teamless_fa_demands(const char* source);
DWORD WINAPI kbo_no_minor_contract_demand_floor_scanner_thread(LPVOID param);
void start_kbo_no_minor_contract_demand_floor_scanner_thread(void);
uint32_t kbo_resolve_submit_offer_screen_player_id(uintptr_t screen_ptr, uint32_t today, uint32_t* out_offset);

#endif
