#include "../../internal/foreign_signability_internal.h"
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

static int kbo_foreign_ai_roster_management_enabled_cached(void)
{
    enum { KBO_FOREIGN_AI_ROSTER_FLAG_CACHE_MS = 500u };
    static volatile LONG s_cached_tick = 0;
    static volatile LONG s_cached_enabled = 0;

    DWORD now = GetTickCount();
    LONG cached_tick = InterlockedCompareExchange(&s_cached_tick, 0, 0);
    if (cached_tick != 0
            && (DWORD)(now - (DWORD)cached_tick) <= KBO_FOREIGN_AI_ROSTER_FLAG_CACHE_MS) {
        return InterlockedCompareExchange(&s_cached_enabled, 0, 0) != 0;
    }

    int enabled = read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_FOREIGN_AI_ROSTER_MANAGEMENT_FILE) ? 1 : 0;
    InterlockedExchange(&s_cached_enabled, enabled);
    InterlockedExchange(&s_cached_tick, (LONG)now);
    return enabled;
}

static int kbo_foreign_reserve_high_value_offer_visible_to_ai(
    uint8_t* player,
    int32_t* out_score,
    int32_t* out_threshold)
{
    int32_t score = 0;
    int32_t threshold = 0;
    if (out_score != NULL) { *out_score = score; }
    if (out_threshold != NULL) { *out_threshold = threshold; }

    if (!kbo_foreign_ai_roster_management_enabled_cached()
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

static int kbo_offer_eligibility_player_is_foreign(uintptr_t player_ptr)
{
    if (player_ptr == 0
            || !memory_range_readable(
                (void*)player_ptr,
                OOTP27_PLAYER_NATION_ID_OFFSET + sizeof(uint32_t))) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    return nation_id != 0u && nation_id != OOTP27_KBO_KOREA_NATION_ID;
}

typedef struct KboOfferEligibilityTeamScopeCacheEntry {
    uintptr_t global;
    uint32_t team_id;
    uint32_t kbo_league_id;
    uint8_t in_scope;
    uint8_t valid;
} KboOfferEligibilityTeamScopeCacheEntry;

enum {
    KBO_OFFER_ELIGIBILITY_TEAM_SCOPE_CACHE_SIZE = 4096
};

static KboOfferEligibilityTeamScopeCacheEntry
    g_kbo_offer_eligibility_team_scope_cache[KBO_OFFER_ELIGIBILITY_TEAM_SCOPE_CACHE_SIZE];
static volatile LONG64 g_kbo_offer_eligibility_cached_global = 0;
static volatile LONG g_kbo_offer_eligibility_cached_kbo_league_id = 0;

static uint32_t kbo_offer_eligibility_kbo_league_id_cached(void)
{
    uintptr_t global = get_ootp_cached_global_database();
    LONG64 cached_global = InterlockedCompareExchange64(
        &g_kbo_offer_eligibility_cached_global,
        0,
        0);
    LONG cached_league_id = InterlockedCompareExchange(
        &g_kbo_offer_eligibility_cached_kbo_league_id,
        0,
        0);
    if (global != 0
            && cached_global == (LONG64)global
            && cached_league_id > 0) {
        return (uint32_t)cached_league_id;
    }

    uint32_t league_id = kbo_resolve_kbo_league_id();
    if (global != 0 && league_id != 0u) {
        InterlockedExchange(&g_kbo_offer_eligibility_cached_kbo_league_id, (LONG)league_id);
        InterlockedExchange64(&g_kbo_offer_eligibility_cached_global, (LONG64)global);
    }
    return league_id;
}

static int kbo_offer_eligibility_requester_in_kbo_scope(uint32_t team_id)
{
    if (team_id == 0u) {
        return 1;
    }

    uint32_t kbo_league_id = kbo_offer_eligibility_kbo_league_id_cached();
    if (kbo_league_id == 0u) {
        return 1;
    }

    uintptr_t global = get_ootp_cached_global_database();
    uint32_t slot = (team_id ^ (team_id >> 8) ^ kbo_league_id)
        & (KBO_OFFER_ELIGIBILITY_TEAM_SCOPE_CACHE_SIZE - 1u);
    KboOfferEligibilityTeamScopeCacheEntry* cached =
        &g_kbo_offer_eligibility_team_scope_cache[slot];
    if (cached->valid
            && cached->global == global
            && cached->team_id == team_id
            && cached->kbo_league_id == kbo_league_id) {
        return cached->in_scope ? 1 : 0;
    }

    int in_scope = 0;
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (team_league_id == kbo_league_id) {
            in_scope = 1;
        } else {
            uint32_t parent_team_id = 0u;
            if (memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
                parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
            }
            if (parent_team_id != 0u) {
                uint8_t* parent_team = find_kbo_team_by_numeric_id_any_league(parent_team_id, 1);
                if (parent_team != NULL
                        && memory_range_readable(parent_team, OOTP27_KBO_TEAM_READABLE_BYTES)
                        && *(uint32_t*)(parent_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) == kbo_league_id) {
                    in_scope = 1;
                }
            }
        }
    }

    cached->valid = 0u;
    cached->global = global;
    cached->team_id = team_id;
    cached->kbo_league_id = kbo_league_id;
    cached->in_scope = in_scope ? 1u : 0u;
    cached->valid = 1u;
    return in_scope;
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

    if (!kbo_offer_eligibility_player_is_foreign(player_ptr)) {
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

    if (team_id > 0 && !kbo_offer_eligibility_requester_in_kbo_scope((uint32_t)team_id)) {
        uint8_t out_of_scope_result = 0u;
        KBO_HOOK_PROFILE_PAUSE(profile_hook);
        if (original_func != NULL) {
            out_of_scope_result = original_func((void*)player_ptr, team_id, flag);
        }
        KBO_HOOK_PROFILE_RESUME(profile_hook);
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility.out_of_scope", out_of_scope_result);
    }

    if (kbo_fast_block_fa_candidate_before_original(player_ptr, team_id, "offer_eligibility", NULL)) {
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
        if (player_id != 0u
                && kbo_foreign_waiver_ai_enabled()
                && kbo_get_foreign_waiver_current_yyyymmdd(&today)
                && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
                && holder_team_id != 0u
                && kbo_foreign_reserve_high_value_offer_visible_to_ai(player, &score, &threshold)) {
            uint32_t retained_on = 0u;
            uint32_t expires_on = 0u;
            (void)kbo_get_active_foreign_waiver_right_dates(
                holder_team_id,
                player_id,
                today,
                &retained_on,
                &expires_on);
            kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);
            kbo_record_recent_foreign_offer_allow(player_id, holder_team_id, today);
            uint8_t adjusted = kbo_foreign_reserve_holder_offer_eligibility(original_result);
            static volatile LONG generic_holder_log_count = 0;
            LONG slot = InterlockedIncrement(&generic_holder_log_count);
            if (slot <= 120) {
                kbo_log_runtimef(
                    "foreign reserve offer eligibility holder-visible generic player=%u holder_team=%u original=%u adjusted=%u flag=%d today=%u retained_on=%u expires_on=%u score=%d threshold=%d",
                    player_id,
                    holder_team_id,
                    (uint32_t)original_result,
                    (uint32_t)adjusted,
                    flag,
                    today,
                    retained_on,
                    expires_on,
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
    if (player_id == 0u || !kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.offer_eligibility", original_result);
    }

    if (kbo_foreign_waiver_ai_enabled()
            && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
            && holder_team_id != 0u) {
        kbo_sync_active_foreign_waiver_right_to_memory(player, player_id, holder_team_id, today);
        if (holder_team_id == (uint32_t)team_id) {
            uint32_t retained_on = 0u;
            uint32_t expires_on = 0u;
            (void)kbo_get_active_foreign_waiver_right_dates(
                holder_team_id,
                player_id,
                today,
                &retained_on,
                &expires_on);
            kbo_record_recent_foreign_offer_allow(player_id, (uint32_t)team_id, today);
            uint8_t adjusted = kbo_foreign_reserve_holder_offer_eligibility(original_result);
            static volatile LONG holder_log_count = 0;
            LONG holder_slot = InterlockedIncrement(&holder_log_count);
            if (holder_slot <= 120) {
                kbo_log_runtimef(
                    "foreign reserve offer eligibility holder adjusted player=%u requester_team=%d holder_team=%u original=%u adjusted=%u flag=%d today=%u retained_on=%u expires_on=%u score=%d",
                    player_id,
                    team_id,
                    holder_team_id,
                    (uint32_t)original_result,
                    (uint32_t)adjusted,
                    flag,
                    today,
                    retained_on,
                    expires_on,
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
        int allowed = kbo_custom_foreign_policy_team_allows_candidate(
            (uint32_t)team_id,
            player,
            &effective_before,
            &effective_after,
            &effective_limit,
            &slot_type,
            &injured_player_id);
        int override_original_block = kbo_custom_foreign_policy_can_override_original_block(player, (uint32_t)team_id);
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
    if (kbo_foreign_injury_replacement_signing_exception_available(
            (uint32_t)team_id,
            player,
            &injury_slot_type,
            &injured_player_id,
            &effective_count,
            &effective_limit)) {
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

