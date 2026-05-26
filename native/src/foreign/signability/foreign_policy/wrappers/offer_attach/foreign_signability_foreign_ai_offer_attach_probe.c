#include "../../internal/foreign_signability_internal.h"
#include "foreign_ai_offer_contract_type.h"
#include "foreign_signability_offer_attach_probe_utils.h"
#include "log/foreign_signability_foreign_ai_offer_attach_probe_log.h"
#include "../../../../../build_verify/build_verify.h"
#include "../../../../../core/core_flags/keys/runtime_flag_keys.generated.h"
#include "../../../submit_offer_probe/submit_offer_probe.h"

typedef void (__fastcall *KboOotpForeignAiOfferAttachFn)(uintptr_t player_ptr, uintptr_t offer_slot_ptr);
typedef uintptr_t (__fastcall *KboOotpForeignAiOfferBuildFn)(
    uintptr_t player_ptr,
    int32_t team_id,
    uintptr_t zero_arg,
    uintptr_t flag_ptr,
    uint8_t stack_flag);
typedef void (__fastcall *KboOotpForeignAiOfferTermsBuildFn)(
    uintptr_t player_ptr,
    uintptr_t terms_ptr,
    int32_t team_id,
    uint8_t offer_flag,
    uint8_t stack_flag_0,
    uint8_t stack_flag_1,
    uint8_t stack_flag_2);
typedef uint8_t (__fastcall *KboOotpForeignAiOfferFinalGateFn)(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary);

static int kbo_foreign_ai_offer_active_baseline_snapshot(
    uintptr_t player_ptr,
    int* out_reserve_right)
{
    if (out_reserve_right != NULL) {
        *out_reserve_right = 0;
    }
    if (player_ptr == 0 || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int active = 0;
    kbo_lock_enter(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) != 0
            && g_kbo_foreign_fa_demand_ladder_snapshot.player_id == player_id) {
        active = 1;
        if (out_reserve_right != NULL) {
            *out_reserve_right =
                g_kbo_foreign_fa_demand_ladder_snapshot.reserve_right != 0u;
        }
    }
    kbo_lock_leave(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);
    return active;
}

static int kbo_foreign_ai_offer_player_has_active_reserve_right(uintptr_t player_ptr)
{
    if (!kbo_foreign_waiver_ai_enabled()
            || player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0u;
    uint32_t holder_team_id = 0u;
    return player_id != 0u
        && kbo_get_foreign_waiver_current_yyyymmdd(&today)
        && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
        && holder_team_id != 0u;
}

__declspec(noinline) void ootp_kbo_foreign_ai_offer_attach_probe_wrapper(
    uintptr_t player_ptr,
    uintptr_t offer_slot_ptr,
    uintptr_t caller_return_ptr,
    uintptr_t original_func_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    KboOotpForeignAiOfferAttachFn original_func = (KboOotpForeignAiOfferAttachFn)original_func_ptr;
    int baseline_active = 0;
    int baseline_reserve_right = 0;
    if (original_func != NULL) {
        kbo_prepare_foreign_fa_offer_demand_baseline(player_ptr, "foreign_ai_offer_attach");
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        original_func(player_ptr, offer_slot_ptr);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        baseline_active =
            kbo_foreign_ai_offer_active_baseline_snapshot(player_ptr, &baseline_reserve_right);
        kbo_restore_foreign_fa_demand_salary_ladder("foreign_ai_offer_attach");
    }
    if (offer_slot_ptr != 0 && memory_range_readable((void*)offer_slot_ptr, sizeof(uintptr_t))) {
        uintptr_t offer_ptr = *(uintptr_t*)offer_slot_ptr;
        kbo_foreign_ai_offer_force_major_contract(
            player_ptr,
            offer_ptr,
            0,
            "foreign_ai_offer_attach");
        if (baseline_active) {
            kbo_foreign_ai_offer_match_demand_salary(
                player_ptr,
                offer_ptr,
                0,
                baseline_reserve_right,
                "foreign_ai_offer_attach");
        }
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

    kbo_prepare_foreign_fa_offer_demand_baseline_for_team_key(player_ptr, team_id, "foreign_ai_offer_build");
    uintptr_t offer_ptr = 0;
    int baseline_active = 0;
    int baseline_reserve_right = 0;
    if (original_func != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        offer_ptr = original_func(player_ptr, team_id, zero_arg, flag_ptr, 0u);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        baseline_active =
            kbo_foreign_ai_offer_active_baseline_snapshot(player_ptr, &baseline_reserve_right);
    }
    kbo_restore_foreign_fa_demand_salary_ladder("foreign_ai_offer_build");
    kbo_foreign_ai_offer_force_major_contract(player_ptr, offer_ptr, team_id, "foreign_ai_offer_build");
    if (baseline_active) {
        kbo_foreign_ai_offer_match_demand_salary(
            player_ptr,
            offer_ptr,
            team_id,
            baseline_reserve_right,
            "foreign_ai_offer_build");
    }
    kbo_log_foreign_ai_offer_build(player_ptr, team_id, flag_ptr, offer_ptr);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_build", offer_ptr);
}

__declspec(noinline) void ootp_kbo_foreign_ai_offer_terms_build_probe_wrapper(
    uintptr_t player_ptr,
    uintptr_t terms_ptr,
    int32_t team_id,
    uint8_t offer_flag,
    uint8_t stack_flag_0,
    uint8_t stack_flag_1,
    uint8_t stack_flag_2)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    KboOotpForeignAiOfferTermsBuildFn original_func =
        (KboOotpForeignAiOfferTermsBuildFn)kbo_offer_probe_resolve_rva(OOTP27_AI_FA_OFFER_TERMS_BUILD_FUNC_RVA);

    kbo_prepare_foreign_fa_offer_demand_baseline_for_team_key(player_ptr, team_id, "foreign_ai_offer_terms");
    int baseline_active = 0;
    int baseline_reserve_right = 0;
    if (original_func != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        original_func(
            player_ptr,
            terms_ptr,
            team_id,
            offer_flag,
            stack_flag_0,
            stack_flag_1,
            stack_flag_2);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        baseline_active =
            kbo_foreign_ai_offer_active_baseline_snapshot(player_ptr, &baseline_reserve_right);
    }
    kbo_foreign_ai_offer_force_major_contract(player_ptr, terms_ptr, team_id, "foreign_ai_offer_terms");
    if (baseline_active) {
        kbo_foreign_ai_offer_match_demand_salary(
            player_ptr,
            terms_ptr,
            team_id,
            baseline_reserve_right,
            "foreign_ai_offer_terms");
    }

    if (baseline_active
            && player_ptr != 0
            && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        static volatile LONG terms_log_count = 0;
        LONG slot = InterlockedIncrement(&terms_log_count);
        if (slot <= 240) {
            kbo_log_runtimef(
                "KBO foreign AI offer terms baseline active player=%u team=%d asian_quota=%u terms=%p salary_primary=%d salary_y1=%d years=%u demand=%d flag=%u stack0=%u stack1=%u stack2=%u",
                player_id,
                team_id,
                (uint32_t)kbo_player_is_asian_quota_slot_candidate(player),
                (void*)terms_ptr,
                kbo_offer_read_i32(terms_ptr, KBO_OFFER_SALARY_PRIMARY_OFFSET),
                kbo_offer_read_i32(terms_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET),
                (uint32_t)kbo_offer_read_u8(terms_ptr, KBO_OFFER_YEAR_COUNT_OFFSET),
                *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET),
                (uint32_t)offer_flag,
                (uint32_t)stack_flag_0,
                (uint32_t)stack_flag_1,
                (uint32_t)stack_flag_2);
        }
    }
    kbo_restore_foreign_fa_demand_salary_ladder("foreign_ai_offer_terms");
    KBO_HOOK_PROFILE_END(profile_hook, "foreign.ai_offer_terms");
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
    int32_t team_id = (int32_t)kbo_offer_probe_team_id_from_ptr(team_ptr);
    int reserve_right = kbo_foreign_ai_offer_player_has_active_reserve_right(player_ptr);
    kbo_foreign_ai_offer_force_major_contract(
        player_ptr,
        offer_ptr,
        team_id,
        "foreign_ai_offer_final_gate");
    kbo_foreign_ai_offer_match_demand_salary(
        player_ptr,
        offer_ptr,
        team_id,
        reserve_right,
        "foreign_ai_offer_final_gate");
    int32_t adjusted_salary = kbo_foreign_ai_offer_adjust_final_gate_salary_arg(
        player_ptr,
        offer_ptr,
        team_id,
        reserve_right,
        salary,
        "foreign_ai_offer_final_gate");
    if (original_func != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        result = original_func(team_ptr, player_ptr, adjusted_salary);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
    }
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_RESEARCH_HOOKS_FILE)
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_ATTACH_PROBE_FILE)) {
        kbo_log_foreign_ai_offer_final_gate(team_ptr, player_ptr, adjusted_salary, offer_ptr, result);
    }
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_final_gate", result);
}
