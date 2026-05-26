#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"
#include "../../../build_verify/build_verify.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/sync/lock.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../injury/api/foreign_injury.h"
#include "../../rights/query/foreign_waiver_rights_query.h"
#include "../../../military_service/military_service.h"
#include "foreign_fa_block_state.h"
#include "submit_offer_probe_state.h"

/* Submit-offer probe hook. Included from native/KBOFix.c. */

typedef void (__fastcall *OotpFaSubmitOfferProbeFn)(void* screen);
typedef int (__fastcall *OotpFaOfferScreenCallbackProbeFn)(void* screen, void* sender, uintptr_t callback_id, uintptr_t value);
typedef int (__fastcall *OotpFaContractOfferCallbackProbeFn)(void* offer, void* sender, uintptr_t callback_id, uintptr_t value);
typedef uint8_t (__fastcall *OotpPlayerActionEligibilityFn)(void* action_context, int32_t action_id, uint8_t strict_check);
typedef uint8_t* (__fastcall *OotpLeagueFinancialsLookupFn)(void* global_db, int32_t league_id);

LONG g_kbo_no_minor_contract_demand_floor_enabled = 0;

void kbo_enable_no_minor_contract_demand_floor(void)
{
    InterlockedExchange(&g_kbo_no_minor_contract_demand_floor_enabled, 1);
}

typedef struct KboFinancialSalaryLadderSnapshot {
    uint8_t* financials;
    int32_t values[9];
    int32_t patched_values[9];
    int32_t demand_ceiling_value;
    int32_t patched_demand_ceiling_value;
    uint32_t player_id;
    uint32_t source_rva;
    uint32_t asian_quota;
    uint32_t reserve_right;
    uint32_t holder_team_id;
    uint32_t today;
    LONG active;
} KboFinancialSalaryLadderSnapshot;

KboFinancialSalaryLadderSnapshot g_kbo_foreign_fa_demand_ladder_snapshot = {0};
KboLock g_kbo_foreign_fa_demand_ladder_snapshot_lock = KBO_LOCK_INIT;
volatile LONG g_kbo_foreign_fa_demand_restore_timer_pending = 0;

const uint32_t KBO_FINANCIALS_SALARY_LADDER_OFFSETS[9] = {
    OOTP27_FINANCIALS_SALARY_LADDER_MINIMUM_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_POOR_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_FAIR_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_BELOW_AVERAGE_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_AVERAGE_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_ABOVE_AVERAGE_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_GOOD_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_STAR_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_SUPERSTAR_OFFSET
};

int32_t kbo_no_minor_resolve_current_league_id(void);
int kbo_no_minor_player_is_teamless_demand_floor_candidate(uint8_t* player, uint32_t league_id);
void kbo_restore_foreign_fa_demand_salary_ladder(const char* source);
uint8_t* kbo_resolve_current_league_financials(uint32_t* out_league_id);

