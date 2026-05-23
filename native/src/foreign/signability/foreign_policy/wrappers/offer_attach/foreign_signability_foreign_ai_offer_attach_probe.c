#include "../../internal/foreign_signability_internal.h"
#include "foreign_signability_offer_attach_probe_utils.h"
#include "log/foreign_signability_foreign_ai_offer_attach_probe_log.h"
#include "../../../../../build_verify/build_verify.h"
#include "../../../../../fa_market_investigation/probe/domestic_fa_offer_probe.h"
#include "../../../../../team/assignment/org_query/team_org_assignment_query.h"
#include "../../../../controller/foreign_ai_controller.h"
#include "../../../api/foreign_signability_salary_floor.h"
#include "../../../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../../../core/core_flags/keys/runtime_flag_keys.generated.h"
#include "../../../../controller/foreign_ai_fast_fill_controller.h"

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

static int kbo_foreign_ai_fast_fill_offer_gate_enabled(void)
{
    enum { KBO_FOREIGN_AI_FAST_FILL_FLAG_CACHE_MS = 500u };
    static volatile LONG s_cached_tick = 0;
    static volatile LONG s_cached_enabled = 0;

    DWORD now = GetTickCount();
    LONG cached_tick = InterlockedCompareExchange(&s_cached_tick, 0, 0);
    if (cached_tick != 0
            && (DWORD)(now - (DWORD)cached_tick) <= KBO_FOREIGN_AI_FAST_FILL_FLAG_CACHE_MS) {
        return InterlockedCompareExchange(&s_cached_enabled, 0, 0) != 0;
    }

    int enabled = kbo_custom_foreign_policy_enabled()
        && (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_MANAGEMENT_FILE)
            || kbo_foreign_ai_controller_enabled())
        && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_DISABLE_KBO_FOREIGN_AI_FAST_FILL_OFFER_GATE_FILE);
    InterlockedExchange(&s_cached_enabled, enabled ? 1 : 0);
    InterlockedExchange(&s_cached_tick, (LONG)now);
    return enabled;
}

static uint8_t kbo_foreign_ai_fast_fill_offer_final_gate(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary,
    uintptr_t offer_ptr,
    uint8_t original_result)
{
    if (original_result != 0u || !kbo_foreign_ai_fast_fill_offer_gate_enabled()) {
        return original_result;
    }
    if (team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return original_result;
    }

    uint32_t team_id = kbo_offer_probe_team_id_from_ptr(team_ptr);
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (team_id == 0u
            || player_id == 0u
            || !kbo_player_is_foreign_for_kbo_rights(player)
            || !kbo_custom_foreign_policy_can_override_original_block(player, team_id)
            || kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)
            || *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != 0u) {
        return original_result;
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today) || today == 0u) {
        return original_result;
    }

    uint32_t pending_asian = 0u;
    uint32_t pending_non_asian = 0u;
    int candidate_pending = 0;
    kbo_custom_foreign_count_pending_offers(
        team_id,
        today,
        player_id,
        &pending_asian,
        &pending_non_asian,
        &candidate_pending);
    if (candidate_pending) {
        return original_result;
    }

    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    int allowed = kbo_custom_foreign_policy_team_allows_candidate(
        team_id,
        player,
        &effective_before,
        &effective_after,
        &effective_limit,
        &slot_type,
        &injured_player_id);

    uint32_t base_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    if (!allowed || base_limit == 0u || effective_after > base_limit) {
        return original_result;
    }

    int candidate_asian = kbo_player_is_asian_quota_slot_candidate(player) ? 1 : 0;
    KboForeignFastFillContext fast_fill = {0};
    if (!kbo_foreign_ai_fast_fill_get_context(team_id, &fast_fill)
            || !kbo_foreign_ai_fast_fill_candidate_solves_context(&fast_fill, candidate_asian)) {
        static volatile LONG skip_log_count = 0;
        LONG skip_slot = InterlockedIncrement(&skip_log_count);
        if (skip_slot <= 200) {
            kbo_log_runtimef(
                "foreign ai fast-fill offer final gate kept original player=%u team=%u original=%u reason=%s candidate_asian=%u vacant=%u effective_vacant=%u asian_quota_vacant=%u effective_with_pending=%u limit=%u today=%u",
                player_id,
                team_id,
                (uint32_t)original_result,
                fast_fill.team_id != 0u ? "candidate_does_not_solve_slot" : "no_fast_fill_context",
                (uint32_t)candidate_asian,
                (uint32_t)fast_fill.vacant,
                (uint32_t)fast_fill.effective_vacant,
                (uint32_t)fast_fill.asian_quota_vacant,
                fast_fill.effective_with_pending,
                fast_fill.limit,
                today);
        }
        return original_result;
    }

    kbo_record_custom_foreign_pending_offer(team_id, player, today);
    kbo_record_recent_custom_foreign_policy_allow(player_id, team_id, today);

    static volatile LONG fast_fill_log_count = 0;
    LONG slot = InterlockedIncrement(&fast_fill_log_count);
    if (slot <= 300) {
        kbo_log_runtimef(
            "foreign ai fast-fill offer final gate adjusted player=%u team=%u original=%u adjusted=1 reason=%s effective_before=%u effective_after=%u limit=%u base_limit=%u pending_asian=%u pending_non_asian=%u asian=%u fast_fill_effective_vacant=%u fast_fill_asian_quota_vacant=%u injury_slot=%s injured=%u salary_arg=%d demand=%d score=%d offer=%p offer_salary=%d offer_years=%u today=%u",
            player_id,
            team_id,
            (uint32_t)original_result,
            fast_fill.asian_quota_vacant && candidate_asian ? "asian_quota_vacant" : "effective_vacant",
            effective_before,
            effective_after,
            effective_limit,
            base_limit,
            pending_asian,
            pending_non_asian,
            (uint32_t)candidate_asian,
            (uint32_t)fast_fill.effective_vacant,
            (uint32_t)fast_fill.asian_quota_vacant,
            slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
            injured_player_id,
            salary,
            *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET),
            kbo_offer_probe_player_value_score(player),
            (void*)offer_ptr,
            kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET),
            (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_YEAR_COUNT_OFFSET),
            today);
    }
    return 1u;
}

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
    result = kbo_foreign_ai_fast_fill_offer_final_gate(team_ptr, player_ptr, salary, offer_ptr, result);
    if (read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_RESEARCH_HOOKS_FILE)
            || read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_AI_OFFER_ATTACH_PROBE_FILE)) {
        kbo_log_foreign_ai_offer_final_gate(team_ptr, player_ptr, salary, offer_ptr, result);
    }
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_offer_final_gate", result);
}
