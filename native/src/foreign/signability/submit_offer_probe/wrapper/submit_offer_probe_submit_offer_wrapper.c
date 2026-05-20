#include "../submit_offer_probe.h"

__declspec(noinline) void ootp_kbo_fa_submit_offer_probe_wrapper(
    uintptr_t screen_ptr,
    uintptr_t original_func_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    uint8_t* custom_policy_player = NULL;
    uint32_t custom_policy_team_id = 0u;
    uint32_t custom_policy_today = 0u;

    kbo_no_minor_clamp_offer_screen_player_salary(screen_ptr, "submit_probe");

    if (screen_ptr != 0 && memory_range_readable((void*)screen_ptr, 0xc0)) {
        uint32_t today = 0;
        uint32_t holder_team_id = 0;
        uint32_t requester_team_id = 0;
        uint32_t allowed_team_id = 0;
        uint32_t player_offset = 0xffffffffu;
        uint32_t player_id = 0;
        if (kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
            player_id = kbo_resolve_submit_offer_screen_player_id(screen_ptr, today, &player_offset);
        }
        static LONG entry_log_count = 0;
        LONG entry_slot = InterlockedIncrement(&entry_log_count);
        if (entry_slot <= 80) {
            kbo_log_runtimef(
                "foreign reserve submit-offer entry: screen=%p player=%u offset=0x%x today=%u cached_player=%ld cached_team=%ld cached_holder=%ld custom_cached_player=%ld custom_cached_team=%ld",
                (void*)screen_ptr,
                player_id,
                player_offset,
                today,
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_player_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_requester_team_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_foreign_offer_block_holder_team_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_player_id, 0, 0),
                InterlockedCompareExchange(&g_kbo_custom_foreign_policy_block_requester_team_id, 0, 0));
        }
        if (player_id != 0u
                && kbo_recent_custom_foreign_policy_block_matches(player_id, today, &requester_team_id)) {
            static LONG custom_submit_block_log_count = 0;
            LONG block_slot = InterlockedIncrement(&custom_submit_block_log_count);
            if (block_slot <= 200) {
                kbo_log_runtimef(
                    "custom foreign policy submit-offer blocked: screen=%p player=%u requester_team=%u today=%u",
                    (void*)screen_ptr,
                    player_id,
                    requester_team_id,
                    today);
            }
            KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_submit_offer");
        }
        if (player_id != 0u
                && kbo_recent_custom_foreign_policy_allow_matches(player_id, today, &allowed_team_id)) {
            requester_team_id = allowed_team_id;
            uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
            if (player != NULL
                    && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                    && kbo_custom_foreign_policy_enabled()
                    && kbo_player_is_foreign_for_kbo_rights(player)) {
                uint32_t effective_before = 0u;
                uint32_t effective_after = 0u;
                uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
                uint8_t slot_type = 0u;
                uint32_t injured_player_id = 0u;
                int allowed = kbo_custom_foreign_policy_team_allows_candidate(
                    allowed_team_id,
                    player,
                    &effective_before,
                    &effective_after,
                    &effective_limit,
                    &slot_type,
                    &injured_player_id);
                if (!allowed) {
                    kbo_record_recent_custom_foreign_policy_block(player_id, allowed_team_id, today);
                    static LONG custom_submit_slot_block_log_count = 0;
                    LONG block_slot = InterlockedIncrement(&custom_submit_slot_block_log_count);
                    if (block_slot <= 200) {
                        kbo_log_runtimef(
                            "custom foreign policy submit-offer blocked by pending count: screen=%p player=%u requester_team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                            (void*)screen_ptr,
                            player_id,
                            allowed_team_id,
                            effective_before,
                            effective_after,
                            effective_limit,
                            slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                            injured_player_id,
                            today);
                    }
                    KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_submit_offer");
                }
                custom_policy_player = player;
                custom_policy_team_id = allowed_team_id;
                custom_policy_today = today;
            }
        } else if (player_id != 0u && kbo_custom_foreign_policy_enabled()) {
            uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
            if (player != NULL
                    && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                    && kbo_player_is_foreign_for_kbo_rights(player)) {
                static LONG custom_submit_missing_allow_log_count = 0;
                LONG missing_slot = InterlockedIncrement(&custom_submit_missing_allow_log_count);
                if (missing_slot <= 120) {
                    kbo_log_runtimef(
                        "custom foreign policy submit-offer missing recent allow: screen=%p player=%u today=%u",
                        (void*)screen_ptr,
                        player_id,
                        today);
                }
            }
        }
        if (player_id != 0u
            && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)) {
            static LONG probe_log_count = 0;
            LONG slot = InterlockedIncrement(&probe_log_count);
            if (slot <= 200) {
                kbo_log_runtimef(
                    "foreign reserve submit-offer probe: screen=%p player=%u holder_team=%u today=%u",
                    (void*)screen_ptr,
                    player_id,
                    holder_team_id,
                    today);
            }
            int recent_block = kbo_recent_foreign_offer_block_matches(player_id, today, &requester_team_id, &holder_team_id);
            int recent_allow = kbo_recent_foreign_offer_allow_matches(player_id, today, &allowed_team_id);
            if ((!recent_block || requester_team_id != holder_team_id)
                    && (!recent_allow || allowed_team_id != holder_team_id)) {
                static LONG submit_block_log_count = 0;
                LONG block_slot = InterlockedIncrement(&submit_block_log_count);
                if (block_slot <= 200) {
                    kbo_log_runtimef(
                        "foreign reserve submit-offer blocked: screen=%p player=%u requester_team=%u holder_team=%u today=%u reason=%s",
                        (void*)screen_ptr,
                        player_id,
                        requester_team_id,
                        holder_team_id,
                        today,
                        recent_block ? "recent_block" : "active_right");
                }
                KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_submit_offer");
            }
            if (custom_policy_team_id == 0u && recent_allow && allowed_team_id == holder_team_id) {
                uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
                if (player != NULL
                        && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                        && kbo_custom_foreign_policy_enabled()
                        && kbo_player_is_foreign_for_kbo_rights(player)) {
                    uint32_t effective_before = 0u;
                    uint32_t effective_after = 0u;
                    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
                    uint8_t slot_type = 0u;
                    uint32_t injured_player_id = 0u;
                    int allowed = kbo_custom_foreign_policy_team_allows_candidate(
                        holder_team_id,
                        player,
                        &effective_before,
                        &effective_after,
                        &effective_limit,
                        &slot_type,
                        &injured_player_id);
                    if (!allowed) {
                        kbo_record_recent_custom_foreign_policy_block(player_id, holder_team_id, today);
                        static LONG holder_submit_slot_block_log_count = 0;
                        LONG block_slot = InterlockedIncrement(&holder_submit_slot_block_log_count);
                        if (block_slot <= 200) {
                            kbo_log_runtimef(
                                "custom foreign policy holder submit-offer blocked by pending count: screen=%p player=%u requester_team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                                (void*)screen_ptr,
                                player_id,
                                holder_team_id,
                                effective_before,
                                effective_after,
                                effective_limit,
                                slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                                injured_player_id,
                                today);
                        }
                        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_submit_offer");
                    }
                    custom_policy_player = player;
                    custom_policy_team_id = holder_team_id;
                    custom_policy_today = today;
                }
            }
        }
    }

    OotpFaSubmitOfferProbeFn original_func = (OotpFaSubmitOfferProbeFn)original_func_ptr;
    if (original_func != NULL) {
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        original_func((void*)screen_ptr);
        KBO_HOOK_PROFILE_RESUME(profile_hook);
    }
    if (custom_policy_player != NULL && custom_policy_team_id != 0u && custom_policy_today != 0u) {
        kbo_record_custom_foreign_pending_offer(custom_policy_team_id, custom_policy_player, custom_policy_today);
    }
    KBO_HOOK_PROFILE_END(profile_hook, "foreign.fa_submit_offer");
}

__declspec(noinline) void ootp_kbo_fa_offer_player_demand_floor_probe(
    uintptr_t player_ptr,
    uintptr_t screen_ptr,
    uint32_t source_rva)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    const char* source = "offer_build";
    if (source_rva == OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17A79BB_RVA) {
        source = "offer_build_17A79BB";
    } else if (source_rva == OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17B50B4_RVA) {
        source = "offer_build_17B50B4";
    }
    kbo_no_minor_clamp_player_demand_salary(player_ptr, screen_ptr, source);
    KBO_HOOK_PROFILE_END(profile_hook, "foreign.fa_offer_demand_floor");
}

