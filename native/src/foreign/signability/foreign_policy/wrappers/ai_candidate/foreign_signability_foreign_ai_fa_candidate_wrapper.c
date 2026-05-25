#include "../../internal/foreign_signability_internal.h"
#include "../../../../../fa_market_investigation/probe/domestic_fa_offer_probe.h"
#include "../candidate_array/foreign_signability_ai_fa_candidate_array.h"
#include "../retained_candidates/foreign_signability_foreign_ai_fa_retained_candidates.h"

/* AI FA status candidate hook wrapper. Included from native/KBOFix.c. */

#define KBO_AI_FA_STATUS_FORCED_REPLACEMENT_MAX 32
static int kbo_ai_fa_status_released_replacement_can_enter_market(uint8_t* player, uint32_t expected_player_id)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) != expected_player_id) {
        return 0;
    }
    if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) != 0u
            || *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) != 0u) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET) != 0u) {
        return 0;
    }
    return kbo_player_is_foreign_for_kbo_rights(player);
}

static int kbo_ai_fa_status_forced_replacement_id_already_collected(
    const uint32_t* player_ids,
    int count,
    uint32_t player_id)
{
    for (int i = 0; i < count; i++) {
        if (player_ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

static void kbo_ai_fa_status_ensure_injury_records_throttled(void)
{
    static volatile LONG ensure_guard = 0;
    static volatile LONG last_ensure_tick = 0;

    DWORD now = GetTickCount();
    LONG last = InterlockedCompareExchange(&last_ensure_tick, 0, 0);
    if (last != 0 && now - (DWORD)last < 5000u) {
        return;
    }

    if (InterlockedCompareExchange(&ensure_guard, 1, 0) != 0) {
        return;
    }
    if (kbo_foreign_injury_replacements_loaded_for_current_save()) {
        kbo_ensure_foreign_injury_replacements_loaded();
    }
    InterlockedExchange(&last_ensure_tick, (LONG)GetTickCount());
    InterlockedExchange(&ensure_guard, 0);
}

static int kbo_ai_fa_status_force_recently_attempted(uintptr_t frame_ptr, uintptr_t candidate_array)
{
    static uintptr_t last_frame_ptr = 0;
    static uintptr_t last_candidate_array = 0;
    static DWORD last_attempt_tick = 0u;

    DWORD now = GetTickCount();
    if (last_frame_ptr == frame_ptr
            && last_candidate_array == candidate_array
            && last_attempt_tick != 0u
            && now - last_attempt_tick < 1000u) {
        return 1;
    }

    last_frame_ptr = frame_ptr;
    last_candidate_array = candidate_array;
    last_attempt_tick = now;
    return 0;
}

static int32_t kbo_ai_fa_status_force_closed_replacement_market_candidates(
    uintptr_t frame_ptr,
    uint32_t requester_team_id,
    uintptr_t candidate_array,
    int32_t insert_index)
{
    if (candidate_array == 0 || insert_index < 0 || !kbo_foreign_injury_replacement_enabled()) {
        return insert_index;
    }

    if (kbo_ai_fa_status_force_recently_attempted(frame_ptr, candidate_array)) {
        return insert_index;
    }

    kbo_ai_fa_status_ensure_injury_records_throttled();

    uint32_t player_ids[KBO_AI_FA_STATUS_FORCED_REPLACEMENT_MAX] = {0};
    int player_count = 0;

    kbo_lock_foreign_injury_replacements();
    int record_count = g_kbo_foreign_injury_replacement_count;
    if (record_count > KBO_FOREIGN_INJURY_REPLACEMENT_MAX) {
        record_count = KBO_FOREIGN_INJURY_REPLACEMENT_MAX;
    }
    for (int i = 0; i < record_count && player_count < KBO_AI_FA_STATUS_FORCED_REPLACEMENT_MAX; i++) {
        const KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
        if (rec->status != KBO_FOREIGN_INJURY_STATUS_CLOSED || rec->replacement_player_id == 0u) {
            continue;
        }
        if (kbo_ai_fa_status_forced_replacement_id_already_collected(
                player_ids,
                player_count,
                rec->replacement_player_id)) {
            continue;
        }
        player_ids[player_count++] = rec->replacement_player_id;
    }
    kbo_unlock_foreign_injury_replacements();

    for (int i = 0; i < player_count; i++) {
        uint32_t forced_player_id = player_ids[i];
        uint8_t* forced_player = kbo_find_player_by_id(forced_player_id, NULL, NULL);
        if (!kbo_ai_fa_status_released_replacement_can_enter_market(forced_player, forced_player_id)) {
            continue;
        }

        uintptr_t forced_player_ptr = (uintptr_t)forced_player;
        if (kbo_ai_fa_status_candidate_array_contains(candidate_array, insert_index, forced_player_ptr)) {
            continue;
        }

        int32_t before_index = insert_index;
        insert_index = kbo_ai_fa_status_insert_candidate_ptr(
            frame_ptr,
            candidate_array,
            insert_index,
            forced_player_ptr);
        if (insert_index != before_index) {
            static LONG force_log_count = 0;
            LONG slot = InterlockedIncrement(&force_log_count);
            if (slot <= 200) {
                kbo_log_runtimef(
                    "foreign injury replacement FA market candidate forced player=%u requester_team=%u index=%d next=%d",
                    forced_player_id,
                    requester_team_id,
                    before_index,
                    insert_index);
            }
        }
    }

    return insert_index;
}

__declspec(noinline) int32_t ootp_kbo_ai_fa_status_candidate_insert_wrapper(
    uintptr_t frame_ptr,
    uintptr_t player_ptr,
    int32_t insert_index,
    uintptr_t candidate_array)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (frame_ptr == 0 || player_ptr == 0 || insert_index < 0) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_fa_status_candidate_insert", insert_index);
    }
    if (!memory_range_readable((void*)player_ptr, OOTP27_PLAYER_ID_OFFSET + sizeof(uint32_t))) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_fa_status_candidate_insert", insert_index);
    }

    uint32_t player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0;
    uint32_t holder_team_id = 0;
    uint32_t requester_team_id = 0;
    uintptr_t team_ptr = *(uintptr_t*)(frame_ptr + OOTP27_AI_FA_STATUS_FRAME_TEAM_PTR_OFFSET);
    if (team_ptr != 0 && memory_range_readable((void*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET), sizeof(uint32_t))) {
        requester_team_id = *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
    }

    if (requester_team_id != 0u
            && kbo_fast_block_fa_candidate_before_original(
                player_ptr,
                (int32_t)requester_team_id,
                "ai_fa_status_candidate",
                &player_id)) {
        insert_index = kbo_ai_fa_status_force_retained_market_candidates(
            frame_ptr,
            requester_team_id,
            candidate_array,
            insert_index);
        insert_index = kbo_ai_fa_status_force_closed_replacement_market_candidates(
            frame_ptr,
            requester_team_id,
            candidate_array,
            insert_index);
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_fa_status_candidate_insert", insert_index);
    }

    if (player_id != 0u
        && requester_team_id != 0u
        && kbo_get_foreign_waiver_current_yyyymmdd(&today)
        && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
        && holder_team_id != 0u
        && holder_team_id != requester_team_id) {
        static LONG block_log_count = 0;
        LONG slot = InterlockedIncrement(&block_log_count);
        if (slot <= 300) {
            kbo_log_runtimef(
                "foreign reserve AI FA status candidate blocked player=%u requester_team=%u holder_team=%u index=%d today=%u",
                player_id,
                requester_team_id,
                holder_team_id,
                insert_index,
                today);
        }
        insert_index = kbo_ai_fa_status_force_retained_market_candidates(
            frame_ptr,
            requester_team_id,
            candidate_array,
            insert_index);
        insert_index = kbo_ai_fa_status_force_closed_replacement_market_candidates(
            frame_ptr,
            requester_team_id,
            candidate_array,
            insert_index);
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_fa_status_candidate_insert", insert_index);
    }

    if (player_id != 0u
            && requester_team_id != 0u
            && kbo_custom_foreign_policy_enabled()
            && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights((uint8_t*)player_ptr)) {
        if (today == 0u && !kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
            today = 0u;
        }
        uint32_t effective_before = 0u;
        uint32_t effective_after = 0u;
        uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
        uint8_t slot_type = 0u;
        uint32_t injured_player_id = 0u;
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            requester_team_id,
            (uint8_t*)player_ptr,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        if (!allowed) {
            static LONG custom_policy_ai_block_log_count = 0;
            LONG slot = InterlockedIncrement(&custom_policy_ai_block_log_count);
            if (slot <= 300) {
                kbo_log_runtimef(
                    "custom foreign policy AI FA status candidate blocked player=%u requester_team=%u index=%d effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u today=%u",
                    player_id,
                    requester_team_id,
                    insert_index,
                    effective_before,
                    effective_after,
                    effective_limit,
                    slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
                    injured_player_id,
                    today);
            }
            kbo_record_recent_custom_foreign_policy_block(player_id, requester_team_id, today);
            insert_index = kbo_ai_fa_status_force_retained_market_candidates(
                frame_ptr,
                requester_team_id,
                candidate_array,
                insert_index);
            insert_index = kbo_ai_fa_status_force_closed_replacement_market_candidates(
                frame_ptr,
                requester_team_id,
                candidate_array,
                insert_index);
            KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_fa_status_candidate_insert", insert_index);
        }
    }

    insert_index = kbo_ai_fa_status_force_retained_market_candidates(
        frame_ptr,
        requester_team_id,
        candidate_array,
        insert_index);
    int32_t before_insert_index = insert_index;
    insert_index = kbo_ai_fa_status_insert_candidate_ptr(
        frame_ptr,
        candidate_array,
        insert_index,
        player_ptr);
    if (today == 0u) {
        kbo_get_foreign_waiver_current_yyyymmdd(&today);
    }
    kbo_domestic_fa_offer_probe_log_candidate(
        (uint8_t*)player_ptr,
        requester_team_id,
        before_insert_index,
        insert_index,
        today);
    insert_index = kbo_ai_fa_status_force_closed_replacement_market_candidates(
        frame_ptr,
        requester_team_id,
        candidate_array,
        insert_index);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.ai_fa_status_candidate_insert", insert_index);
}

