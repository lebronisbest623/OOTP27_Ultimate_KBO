#include "../../internal/foreign_signability_internal.h"
#include "foreign_signability_offer_attach_probe_utils.h"
#include "log/foreign_signability_foreign_ai_offer_attach_probe_log.h"
#include "../../../../../build_verify/build_verify.h"
#include "../../../api/foreign_signability_salary_floor.h"
#include "../../../../../core/core_flags/keys/runtime_flag_keys.generated.h"

typedef void (__fastcall *KboOotpForeignAiOfferAttachFn)(uintptr_t player_ptr, uintptr_t offer_slot_ptr);
typedef uintptr_t (__fastcall *KboOotpForeignAiOfferBuildFn)(
    uintptr_t player_ptr,
    int32_t team_id,
    uintptr_t zero_arg,
    uintptr_t flag_ptr,
    uint8_t stack_flag);
typedef uint8_t (__fastcall *KboOotpForeignAiOfferFinalGateFn)(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary);

void kbo_prepare_foreign_fa_offer_demand_baseline(uintptr_t player_ptr, const char* source);
void kbo_restore_foreign_fa_demand_salary_ladder(const char* source);

__declspec(noinline) void ootp_kbo_foreign_ai_offer_attach_probe_wrapper(
    uintptr_t player_ptr,
    uintptr_t offer_slot_ptr,
    uintptr_t caller_return_ptr,
    uintptr_t original_func_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    KboOotpForeignAiOfferAttachFn original_func = (KboOotpForeignAiOfferAttachFn)original_func_ptr;
    if (original_func != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        original_func(player_ptr, offer_slot_ptr);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
    }
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_RESEARCH_HOOKS_FILE)
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_ATTACH_PROBE_FILE)) {
        kbo_log_foreign_ai_offer_attach(player_ptr, offer_slot_ptr, caller_return_ptr);
    }
    KBO_HOOK_PROFILE_END(profile_hook, "foreign.ai_offer_attach");
}

__declspec(noinline) uintptr_t ootp_kbo_foreign_ai_offer_build_probe_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    uintptr_t zero_arg,
    uintptr_t flag_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    KboOotpForeignAiOfferBuildFn original_func =
        (KboOotpForeignAiOfferBuildFn)kbo_offer_probe_resolve_rva(OOTP27_AI_FA_OFFER_BUILD_FUNC_RVA);

    uintptr_t offer_ptr = 0;
    if (original_func != NULL) {
        kbo_apply_foreign_contract_demand_floor(player_ptr, 0u, "ai_offer_build");
        kbo_prepare_foreign_fa_offer_demand_baseline(player_ptr, "ai_offer_build");
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        offer_ptr = original_func(player_ptr, team_id, zero_arg, flag_ptr, 0u);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        kbo_restore_foreign_fa_demand_salary_ladder("ai_offer_build");
    }
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_RESEARCH_HOOKS_FILE)
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_ATTACH_PROBE_FILE)) {
        kbo_log_foreign_ai_offer_build(player_ptr, team_id, flag_ptr, offer_ptr);
    }
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_build", offer_ptr);
}

__declspec(noinline) uint8_t ootp_kbo_foreign_ai_offer_final_gate_probe_wrapper(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary,
    uintptr_t offer_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    KboOotpForeignAiOfferFinalGateFn original_func =
        (KboOotpForeignAiOfferFinalGateFn)kbo_offer_probe_resolve_rva(OOTP27_AI_FA_OFFER_FINAL_GATE_FUNC_RVA);

    uint8_t result = 0u;
    if (original_func != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        result = original_func(team_ptr, player_ptr, salary);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
    }
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_RESEARCH_HOOKS_FILE)
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_ATTACH_PROBE_FILE)) {
        kbo_log_foreign_ai_offer_final_gate(team_ptr, player_ptr, salary, offer_ptr, result);
    }
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_final_gate", result);
}
