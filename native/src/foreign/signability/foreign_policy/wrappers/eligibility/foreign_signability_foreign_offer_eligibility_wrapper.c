#include "../../internal/foreign_signability_internal.h"
#include "foreign_offer_eligibility_wrapper_internal.h"
#include "../../../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../../../core/core_flags/keys/runtime_flag_keys.generated.h"

/* Player offer-eligibility hook wrapper. Included from native/KBOFix.c. */

static uint8_t kbo_foreign_reserve_holder_offer_eligibility(uint8_t original_result)
{
    uint8_t adjusted = original_result != 0u ? original_result : 4u;
    if (adjusted > 0u && adjusted < 4u) {
        adjusted = 4u;
    }
    return adjusted;
}


__declspec(noinline) uint8_t ootp_kbo_player_offer_eligibility_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    int32_t flag,
    uintptr_t original_func_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    typedef uint8_t (__fastcall *OriginalOfferEligibilityFn)(void*, int32_t, int32_t);
    OriginalOfferEligibilityFn original_func = (OriginalOfferEligibilityFn)original_func_ptr;

    if (kbo_runtime_save_in_progress()) {
        uint8_t save_result = 0u;
        if (original_func != NULL) {
            KBO_HOOK_PROFILE_PAUSE(profile_hook);
            save_result = original_func((void*)player_ptr, team_id, flag);
            KBO_HOOK_PROFILE_RESUME(profile_hook);
        }
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", save_result);
    }

    KBO_PROFILE_BEGIN(profile_offer_player_is_foreign);
    int player_is_foreign = kbo_offer_eligibility_player_is_foreign(player_ptr);
    KBO_PROFILE_END(profile_offer_player_is_foreign, "foreign_policy.offer_eligibility.player_is_foreign");
    if (!player_is_foreign) {
        uint8_t non_foreign_result = 0u;
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        KBO_PROFILE_BEGIN(profile_foreign_offer_original);
        if (original_func != NULL) {
            non_foreign_result = original_func((void*)player_ptr, team_id, flag);
        }
        KBO_PROFILE_END(profile_foreign_offer_original, "foreign_policy.offer_eligibility.original");
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", non_foreign_result);
    }

    int requester_in_scope = 1;
    if (team_id > 0) {
        KBO_PROFILE_BEGIN(profile_offer_team_scope);
        requester_in_scope = kbo_offer_eligibility_requester_in_kbo_scope((uint32_t)team_id);
        KBO_PROFILE_END(profile_offer_team_scope, "foreign_policy.offer_eligibility.team_scope");
    }
    if (team_id > 0 && !requester_in_scope) {
        uint8_t out_of_scope_result = 0u;
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        if (original_func != NULL) {
            out_of_scope_result = original_func((void*)player_ptr, team_id, flag);
        }
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility.out_of_scope", out_of_scope_result);
    }

    KBO_PROFILE_BEGIN(profile_offer_fast_block);
    int fast_blocked = kbo_fast_block_fa_candidate_before_original(player_ptr, team_id, "offer_eligibility", NULL);
    KBO_PROFILE_END(profile_offer_fast_block, "foreign_policy.offer_eligibility.fast_block");
    if (fast_blocked) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", 0u);
    }

    uint8_t original_result = 0;
    KBO_HOOK_PROFILE_PAUSE(profile_hook);
    KBO_PROFILE_BEGIN(profile_foreign_offer_original);
    if (original_func != NULL) {
        original_result = original_func((void*)player_ptr, team_id, flag);
    }
    KBO_PROFILE_END(profile_foreign_offer_original, "foreign_policy.offer_eligibility.original");
    KBO_HOOK_PROFILE_RESUME(profile_hook);

    if (player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", original_result);
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (team_id <= 0) {
        uint32_t today = 0u;
        uint32_t holder_team_id = 0u;
        int32_t score = 0;
        int32_t threshold = 0;
        KBO_PROFILE_BEGIN(profile_offer_generic_holder_probe);
        int generic_holder_visible = player_id != 0u
                && kbo_foreign_waiver_ai_enabled()
                && kbo_get_foreign_waiver_current_yyyymmdd(&today)
                && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
                && holder_team_id != 0u
                && kbo_foreign_reserve_high_value_offer_visible_to_ai(player, &score, &threshold);
        KBO_PROFILE_END(profile_offer_generic_holder_probe, "foreign_policy.offer_eligibility.generic_holder_probe");
        if (generic_holder_visible) {
            kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);
            kbo_record_recent_foreign_offer_allow(player_id, holder_team_id, today);
            uint8_t adjusted = kbo_foreign_reserve_holder_offer_eligibility(original_result);
            static volatile LONG generic_holder_log_count = 0;
            LONG slot = InterlockedIncrement(&generic_holder_log_count);
            if (slot <= 120) {
                kbo_log_runtimef(
                    "foreign reserve offer eligibility holder-visible generic player=%u holder_team=%u original=%u adjusted=%u flag=%d today=%u score=%d threshold=%d",
                    player_id,
                    holder_team_id,
                    (uint32_t)original_result,
                    (uint32_t)adjusted,
                    flag,
                    today,
                    score,
                    threshold);
            }
            KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", adjusted);
        }
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", original_result);
    }

    kbo_log_asian_quota_offer_probe(player, player_id, team_id, original_result, flag);

    uint32_t today = 0u;
    uint32_t holder_team_id = 0u;
    int has_today = 0;
    if (player_id != 0u) {
        KBO_PROFILE_BEGIN(profile_offer_current_date);
        has_today = kbo_get_foreign_waiver_current_yyyymmdd(&today);
        KBO_PROFILE_END(profile_offer_current_date, "foreign_policy.offer_eligibility.current_date");
    }
    if (player_id == 0u || !has_today) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", original_result);
    }

    int has_active_holder = 0;
    if (kbo_foreign_waiver_ai_enabled()) {
        KBO_PROFILE_BEGIN(profile_offer_holder_lookup);
        has_active_holder = kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
            && holder_team_id != 0u;
        KBO_PROFILE_END(profile_offer_holder_lookup, "foreign_policy.offer_eligibility.holder_lookup");
    }
    if (has_active_holder) {
        kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);
        if (holder_team_id == (uint32_t)team_id) {
            kbo_record_recent_foreign_offer_allow(player_id, (uint32_t)team_id, today);
            uint8_t adjusted = kbo_foreign_reserve_holder_offer_eligibility(original_result);
            static volatile LONG holder_log_count = 0;
            LONG holder_slot = InterlockedIncrement(&holder_log_count);
            if (holder_slot <= 120) {
                kbo_log_runtimef(
                    "foreign reserve offer eligibility holder adjusted player=%u requester_team=%d holder_team=%u original=%u adjusted=%u flag=%d today=%u score=%d",
                    player_id,
                    team_id,
                    holder_team_id,
                    (uint32_t)original_result,
                    (uint32_t)adjusted,
                    flag,
                    today,
                    kbo_foreign_waiver_value_score(player));
            }
            KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", adjusted);
        }

        static volatile LONG log_count = 0;
        LONG slot = InterlockedIncrement(&log_count);
        if (slot <= 80) {
            kbo_log_runtimef(
                "foreign reserve offer eligibility blocked player=%u requester_team=%d holder_team=%u original=%u flag=%d today=%u",
                player_id,
                team_id,
                holder_team_id,
                (uint32_t)original_result,
                flag,
                today);
        }

        kbo_record_recent_foreign_offer_block(player_id, (uint32_t)team_id, holder_team_id, today);
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", 0u);
    }

    if (kbo_custom_foreign_policy_enabled() && kbo_player_is_foreign_for_kbo_rights(player)) {
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        KBO_PROFILE_BEGIN(profile_offer_custom_allows);
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            (uint32_t)team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        KBO_PROFILE_END(profile_offer_custom_allows, "foreign_policy.offer_eligibility.custom_policy_allows");
        KBO_PROFILE_BEGIN(profile_offer_custom_override);
        int override_original_block = kbo_custom_foreign_policy_can_override_original_block(player, (uint32_t)team_id);
        KBO_PROFILE_END(profile_offer_custom_override, "foreign_policy.offer_eligibility.custom_policy_override");
        uint8_t adjusted = allowed ? original_result : 0u;
        if (allowed && original_result == 0u && override_original_block) {
            adjusted = 4u;
        }
        if (adjusted > 0u && adjusted < 4u) {
            adjusted = 4u;
        }

        static LONG custom_policy_offer_log_enabled_initialized = 0;
        static LONG custom_policy_offer_log_enabled = 0;
        if (InterlockedCompareExchange(&custom_policy_offer_log_enabled_initialized, 1, 0) == 0) {
            InterlockedExchange(
                &custom_policy_offer_log_enabled,
                read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_CUSTOM_FOREIGN_OFFER_LOGS_FILE) ? 1 : 0);
        }
        static volatile LONG custom_policy_offer_log_count = 0;
        LONG offer_log_slot = InterlockedIncrement(&custom_policy_offer_log_count);
        if (offer_log_slot <= 120 || InterlockedCompareExchange(&custom_policy_offer_log_enabled, 0, 0) != 0) {
            if (offer_log_slot <= 240) {
                kbo_log_runtimef(
                    "custom foreign policy offer player=%u requester_team=%d original=%u adjusted=%u allowed=%d override=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u flag=%d today=%u",
                    player_id,
                    team_id,
                    (uint32_t)original_result,
                    (uint32_t)adjusted,
                    allowed,
                    override_original_block,
                    effective_before,
                    effective_after,
                    effective_limit,
                    slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                    injured_player_id,
                    flag,
                    today);
            }
        }
        if (!allowed) {
            kbo_record_recent_custom_foreign_policy_block(player_id, (uint32_t)team_id, today);
        } else if (adjusted != 0u && flag != 0) {
            kbo_record_custom_foreign_pending_offer((uint32_t)team_id, player, today);
            kbo_record_recent_custom_foreign_policy_allow(player_id, (uint32_t)team_id, today);
        } else if (adjusted != 0u) {
            kbo_record_recent_custom_foreign_policy_allow(player_id, (uint32_t)team_id, today);
        }
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", adjusted);
    }

    uint8_t injury_slot_type = 0u;
    uint32_t injured_player_id = 0u;
    uint32_t effective_count = 0u;
    uint32_t effective_limit = 0u;
    KBO_PROFILE_BEGIN(profile_offer_injury_exception);
    int injury_exception_available = kbo_foreign_injury_replacement_signing_exception_available(
            (uint32_t)team_id,
            player,
            &injury_slot_type,
            &injured_player_id,
            &effective_count,
            &effective_limit);
    KBO_PROFILE_END(profile_offer_injury_exception, "foreign_policy.offer_eligibility.injury_exception");
    if (injury_exception_available) {
        static volatile LONG injury_offer_log_count = 0;
        LONG slot = InterlockedIncrement(&injury_offer_log_count);
        uint8_t adjusted = original_result != 0u ? original_result : 4u;
        if (adjusted > 0u && adjusted < 4u) {
            adjusted = 4u;
        }
        if (slot <= 120) {
            kbo_log_runtimef(
                "foreign injury replacement offer eligibility allowed player=%u requester_team=%d injured=%u slot=%s original=%u adjusted=%u effective=%u limit=%u flag=%d today=%u",
                player_id,
                team_id,
                injured_player_id,
                kbo_foreign_injury_slot_label(injury_slot_type),
                (uint32_t)original_result,
                (uint32_t)adjusted,
                effective_count,
                effective_limit,
                flag,
                today);
        }
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", adjusted);
    }

    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", original_result);
}

