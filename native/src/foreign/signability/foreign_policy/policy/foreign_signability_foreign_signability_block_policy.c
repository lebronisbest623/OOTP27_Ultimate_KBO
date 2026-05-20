#include "../internal/foreign_signability_internal.h"
#include "foreign_signability_reserve_log.h"
#include "../../../../core/core_flags/keys/runtime_flag_keys.generated.h"

/* Foreign-player signability block and adjustment policy. */

int kbo_no_minor_contract_signability_floor(int signability)
{
    if (!kbo_fix_enabled()) {
        return signability;
    }
    if (signability > 0 && signability < 4) {
        return 4;
    }
    return signability;
}

static int kbo_foreign_reserve_holder_signability(int original_signability)
{
    int adjusted = original_signability != 0 ? original_signability : 4;
    adjusted = kbo_no_minor_contract_signability_floor(adjusted);
    if (adjusted > 0 && adjusted < 4) {
        adjusted = 4;
    }
    return adjusted;
}

static int kbo_foreign_reserve_high_value_retention_visible_to_ai(
    uint8_t* player,
    int32_t* out_score,
    int32_t* out_threshold)
{
    int32_t score = 0;
    int32_t threshold = 0;
    if (out_score != NULL) { *out_score = score; }
    if (out_threshold != NULL) { *out_threshold = threshold; }

    if (!read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_MANAGEMENT_FILE)
            || player == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }

    score = kbo_foreign_waiver_value_score(player);
    threshold = kbo_get_foreign_waiver_value_threshold_for_player(player);
    if (out_score != NULL) { *out_score = score; }
    if (out_threshold != NULL) { *out_threshold = threshold; }
    return score >= threshold;
}

static int kbo_custom_foreign_policy_adjust_signability_for_team(
    uint8_t* player,
    uint32_t player_id,
    uint32_t team_id,
    int original_signability,
    uint32_t today,
    uintptr_t caller_rva,
    int record_block,
    int record_allow,
    const char* source,
    int* out_adjusted)
{
    if (out_adjusted != NULL) { *out_adjusted = original_signability; }
    if (team_id == 0u || player == NULL || !kbo_custom_foreign_policy_enabled()
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
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
    int override_original_block = kbo_custom_foreign_policy_can_override_original_block(player, team_id);
    int adjusted = allowed ? original_signability : 0;
    if (allowed && original_signability == 0 && override_original_block) {
        adjusted = 4;
    }
    adjusted = kbo_no_minor_contract_signability_floor(adjusted);

    static volatile LONG custom_policy_signability_log_count = 0;
    LONG slot = InterlockedIncrement(&custom_policy_signability_log_count);
    if (slot <= 240) {
        kbo_log_runtimef(
            "custom foreign policy signability source=%s player=%u requester_team=%u original=%d adjusted=%d allowed=%d override=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u caller_rva=0x%llx",
            source != NULL ? source : "",
            player_id,
            team_id,
            original_signability,
            adjusted,
            allowed,
            override_original_block,
            effective_before,
            effective_after,
            effective_limit,
            slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
            injured_player_id,
            today,
            (unsigned long long)caller_rva);
    }

    if (!allowed) {
        if (record_block) {
            kbo_record_recent_custom_foreign_policy_block(player_id, team_id, today);
        }
    } else if (adjusted != 0 && record_allow) {
        kbo_record_recent_custom_foreign_policy_allow(player_id, team_id, today);
    }
    if (out_adjusted != NULL) { *out_adjusted = adjusted; }
    return 1;
}

static int kbo_custom_foreign_policy_adjust_generic_signability(
    uint8_t* player,
    uint32_t player_id,
    int original_signability,
    uint32_t today,
    uintptr_t caller_rva,
    int* out_adjusted)
{
    if (out_adjusted != NULL) { *out_adjusted = original_signability; }
    uint32_t team_ids[16] = {0};
    int team_count = kbo_collect_human_controlled_team_ids(
        team_ids,
        (int)(sizeof(team_ids) / sizeof(team_ids[0])),
        "foreign_signability_generic");
    for (int i = 0; i < team_count; i++) {
        int adjusted = original_signability;
        if (!kbo_custom_foreign_policy_adjust_signability_for_team(
                player,
                player_id,
                team_ids[i],
                original_signability,
                today,
                caller_rva,
                0,
                1,
                "generic_display",
                &adjusted)) {
            continue;
        }
        if (adjusted != 0) {
            if (out_adjusted != NULL) { *out_adjusted = adjusted; }
            return 1;
        }
    }
    return 0;
}

int kbo_enforce_foreign_waiver_signability(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    uint16_t year_hint,
    int original_signability,
    uintptr_t caller_rva)
{
    (void)year_hint;
    if (!kbo_fix_enabled()) {
        return original_signability;
    }
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return original_signability;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))) {
        return original_signability;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return original_signability;
    }

    kbo_log_asian_quota_signability_probe(player, player_id, requesting_team_id, original_signability, caller_rva);

    uint32_t today = 0;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        return original_signability;
    }

    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return kbo_no_minor_contract_signability_floor(original_signability);
    }

    uint32_t holder_team_id = 0;
    if (kbo_foreign_waiver_ai_enabled()) {
        kbo_prune_expired_foreign_waiver_rights(today);

        if (kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)) {
            kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);

            if (requesting_team_id <= 0) {
                int32_t score = 0;
                int32_t threshold = 0;
                int holder_visible = kbo_foreign_reserve_high_value_retention_visible_to_ai(
                    player,
                    &score,
                    &threshold);
                if (holder_visible) {
                    int adjusted = kbo_foreign_reserve_holder_signability(original_signability);
                    kbo_record_recent_foreign_offer_allow(player_id, holder_team_id, today);
                    kbo_log_foreign_reserve_holder_visible_generic_signability(
                        player,
                        player_id,
                        holder_team_id,
                        original_signability,
                        adjusted,
                        today,
                        caller_rva,
                        score,
                        threshold);
                    return adjusted;
                }

                kbo_log_foreign_reserve_blocked_generic_signability(
                    player,
                    player_id,
                    holder_team_id,
                    original_signability,
                    today,
                    caller_rva,
                    score,
                    threshold);
                return 0; /* OOTP signability enum: 0 = Impossible. */
            }

            uint32_t team_id = (uint32_t)requesting_team_id;
            if (caller_rva == OOTP27_FOREIGN_SIGNABILITY_FA_LIST_DISPLAY_CALLER_RVA
                    && team_id != holder_team_id) {
                kbo_log_foreign_reserve_blocked_display_signability(
                    player,
                    player_id,
                    team_id,
                    holder_team_id,
                    original_signability,
                    today,
                    caller_rva);
                kbo_record_recent_foreign_offer_block(player_id, team_id, holder_team_id, today);
                return 0; /* This callsite is a FA-list display probe, not a reliable holder-team offer check. */
            }

            if (team_id == holder_team_id) {
                kbo_record_recent_foreign_offer_allow(player_id, team_id, today);
                static LONG allow_log_count = 0;
                LONG slot = InterlockedIncrement(&allow_log_count);
                int adjusted = kbo_foreign_reserve_holder_signability(original_signability);
                if (slot <= 80) {
                    kbo_log_runtimef(
                        "foreign reserve signability: holder adjusted player=%u team=%u original=%d adjusted=%d today=%u caller_rva=0x%llx score=%d",
                        player_id,
                        team_id,
                        original_signability,
                        adjusted,
                        today,
                        (unsigned long long)caller_rva,
                        kbo_foreign_waiver_value_score(player));
                }
                return adjusted;
            }

            static LONG block_log_count = 0;
            LONG slot = InterlockedIncrement(&block_log_count);
            if (slot <= 200) {
                kbo_log_runtimef(
                    "foreign reserve signability: blocked player=%u requester_team=%u holder_team=%u original=%d forced=0 today=%u caller_rva=0x%llx",
                    player_id,
                    team_id,
                    holder_team_id,
                    original_signability,
                    today,
                    (unsigned long long)caller_rva);
            }
            kbo_record_recent_foreign_offer_block(player_id, team_id, holder_team_id, today);
            kbo_log_foreign_signability_block_callsite(caller_rva, player_id, team_id, holder_team_id, original_signability, today);
            return 0; /* OOTP signability enum: 0 = Impossible. */
        }
    }

    if (requesting_team_id > 0
            && kbo_custom_foreign_policy_enabled()
            && kbo_player_is_foreign_for_kbo_rights(player)) {
        int adjusted = original_signability;
        if (kbo_custom_foreign_policy_adjust_signability_for_team(
                player,
                player_id,
                (uint32_t)requesting_team_id,
                original_signability,
                today,
                caller_rva,
                1,
                1,
                "explicit_team",
                &adjusted)) {
            return adjusted;
        }
    }

    if (requesting_team_id <= 0
            && kbo_custom_foreign_policy_enabled()
            && kbo_player_is_foreign_for_kbo_rights(player)) {
        int adjusted = original_signability;
        if (kbo_custom_foreign_policy_adjust_generic_signability(
                player,
                player_id,
                original_signability,
                today,
                caller_rva,
                &adjusted)) {
            return adjusted;
        }
    }

    if (requesting_team_id > 0) {
        uint8_t injury_slot_type = 0u;
        uint32_t injured_player_id = 0u;
        uint32_t effective_count = 0u;
        uint32_t effective_limit = 0u;
        if (kbo_foreign_injury_replacement_signing_exception_available(
                (uint32_t)requesting_team_id,
                player,
                &injury_slot_type,
                &injured_player_id,
                &effective_count,
                &effective_limit)) {
            static LONG injury_signability_log_count = 0;
            LONG slot = InterlockedIncrement(&injury_signability_log_count);
            int adjusted = original_signability != 0 ? original_signability : 4;
            adjusted = kbo_no_minor_contract_signability_floor(adjusted);
            if (slot <= 120) {
                kbo_log_runtimef(
                    "foreign injury replacement signability allowed player=%u requester_team=%d injured=%u slot=%s original=%d adjusted=%d effective=%u limit=%u today=%u caller_rva=0x%llx",
                    player_id,
                    requesting_team_id,
                    injured_player_id,
                    kbo_foreign_injury_slot_label(injury_slot_type),
                    original_signability,
                    adjusted,
                    effective_count,
                    effective_limit,
                    today,
                    (unsigned long long)caller_rva);
            }
            return adjusted;
        }
    }

    return kbo_no_minor_contract_signability_floor(original_signability);
}

