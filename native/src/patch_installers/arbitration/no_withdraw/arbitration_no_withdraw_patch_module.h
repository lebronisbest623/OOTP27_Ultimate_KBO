#ifndef KBOFIX_SRC_PATCH_INSTALLERS_ARBITRATION_NO_WITHDRAW_ARBITRATION_NO_WITHDRAW_PATCH_MODULE_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_ARBITRATION_NO_WITHDRAW_ARBITRATION_NO_WITHDRAW_PATCH_MODULE_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../fa_filing/fa_filing.h"
#include "../../../foreign/common/dates/foreign_waiver_date.h"
#include "../../../foreign/signability/state/submit_offer_probe_state.h"
#include "../../../hook_stubs/military/hook_stubs_military.h"
#include "../../../patch_helpers/patch_helpers.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "arbitration_no_withdraw_patch.h"
#include "../common/arbitration_offer_floor_stubs.h"
#include "../common/arbitration_patch_helpers.h"
typedef void (__fastcall *OotpArbitrationNonTenderFn)(void* team, void* player, uint8_t notify);
typedef uint8_t* (__fastcall *KboArbitrationLeagueFinancialsLookupFn)(void* global_db, int32_t league_id);


int32_t kbo_salary_arbitration_resolve_minimum_salary(uint32_t league_id);
uintptr_t kbo_salary_arbitration_caller_rva(void* return_address);
int kbo_salary_arbitration_is_known_non_tender_return(uintptr_t caller_rva);
__declspec(noinline) void ootp_kbo_salary_arbitration_non_tender_wrapper(
    void* team_ptr,
    void* player_ptr,
    uint8_t notify,
    OotpArbitrationNonTenderFn original_func);
uint8_t* build_kbo_salary_arbitration_non_tender_detour_stub(void* original_trampoline);
int patch_kbo_salary_arbitration_r11_detour_at(
    const char* label,
    uint8_t* target,
    const uint8_t* expected,
    size_t size,
    uint8_t* stub);
int install_kbo_salary_arbitration_non_tender_function_patch(HMODULE exe);
int install_kbo_salary_arbitration_no_withdraw_patch(void);

#endif
