#include "../internal/foreign_signability_internal.h"
#include "../../../quota/candidates/cache/foreign_quota_candidate_limit_cache.h"

/* Early FA candidate block policy. */

int kbo_fast_block_fa_candidate_before_original(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    const char* context,
    uint32_t* out_player_id)
{
    KBO_PROFILE_BEGIN(profile_fa_fast_block);
    if (out_player_id != NULL) {
        *out_player_id = 0u;
    }
    if (!kbo_fix_enabled() || requesting_team_id <= 0) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.disabled");
        return 0;
    }

    if (player_ptr == 0
            || !memory_range_readable(
                (void*)player_ptr,
                OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.bad_player");
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (out_player_id != NULL) {
        *out_player_id = player_id;
    }
    if (player_id == 0u) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.bad_player_id");
        return 0;
    }

    uint32_t requester_team_id = (uint32_t)requesting_team_id;
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    int candidate_is_foreign = nation_id != 0u && nation_id != OOTP27_KBO_KOREA_NATION_ID;
    if (!candidate_is_foreign) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.allowed");
        return 0;
    }

    if (!kbo_player_pointer_plausible(player_ptr)) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.bad_player");
        return 0;
    }

    int waiver_ai_enabled = kbo_foreign_waiver_ai_enabled();
    int custom_policy_enabled = kbo_custom_foreign_policy_enabled();

    if (!waiver_ai_enabled && !custom_policy_enabled) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.allowed");
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.no_date");
        return 0;
    }

    if (waiver_ai_enabled) {
        kbo_prune_expired_foreign_waiver_rights(today);

        uint32_t holder_team_id = 0u;
        if (kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
                && holder_team_id != 0u
                && holder_team_id != requester_team_id) {
            kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);
            kbo_record_recent_foreign_offer_block(player_id, requester_team_id, holder_team_id, today);

            static volatile LONG rights_fast_block_log_count = 0;
            LONG slot = InterlockedIncrement(&rights_fast_block_log_count);
            if (slot <= 40) {
                kbo_log_runtimef(
                    "FA fast-block source=%s reason=foreign_reserve player=%u requester_team=%u holder_team=%u today=%u",
                    context != NULL ? context : "",
                    player_id,
                    requester_team_id,
                    holder_team_id,
                    today);
            } else if (slot == 41) {
                kbo_log_runtime_line("FA fast-block foreign_reserve log suppressed after 40 entries");
            }
            KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.reserve_blocked");
            return 1;
        }
    }

    if (custom_policy_enabled) {
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            requester_team_id,
            player,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL);
        if (!allowed) {
            uint32_t effective_before = 0u;
            uint32_t effective_after = 0u;
            uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
            uint8_t slot_type = 0u;
            uint32_t injured_player_id = 0u;
            int cached_allowed = 1;
            if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
                    || !kbo_custom_foreign_candidate_cache_hit(
                        requester_team_id,
                        player,
                        player_id,
                        today,
                        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
                        &effective_before,
                        &effective_after,
                        &effective_limit,
                        &slot_type,
                        &injured_player_id,
                        &cached_allowed)
                    || cached_allowed) {
                kbo_custom_foreign_policy_team_allows_candidate(
                    requester_team_id,
                    player,
                    &effective_before,
                    &effective_after,
                    &effective_limit,
                    &slot_type,
                    &injured_player_id);
            }
            kbo_record_recent_custom_foreign_policy_block(player_id, requester_team_id, today);
            static volatile LONG custom_fast_block_log_count = 0;
            LONG slot = InterlockedIncrement(&custom_fast_block_log_count);
            if (slot <= 40) {
                kbo_log_runtimef(
                    "FA fast-block source=%s reason=custom_foreign_policy player=%u requester_team=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    context != NULL ? context : "",
                    player_id,
                    requester_team_id,
                    effective_before,
                    effective_after,
                    effective_limit,
                    slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                    injured_player_id,
                    today);
            } else if (slot == 41) {
                kbo_log_runtime_line("FA fast-block custom_foreign_policy log suppressed after 40 entries");
            }
            KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.custom_blocked");
            return 1;
        }
    }

    KBO_PROFILE_END(profile_fa_fast_block, "foreign_policy.fast_block.allowed");
    return 0;
}

