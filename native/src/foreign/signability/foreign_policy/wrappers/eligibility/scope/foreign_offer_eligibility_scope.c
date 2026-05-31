#include "../foreign_offer_eligibility_wrapper_internal.h"
#include "../../../internal/foreign_signability_internal.h"
#include "../../../../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../../../../core/core_flags/keys/runtime_flag_keys.generated.h"

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

int kbo_foreign_reserve_high_value_offer_visible_to_ai(
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

int kbo_offer_eligibility_player_is_foreign(uintptr_t player_ptr)
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

int kbo_offer_eligibility_requester_in_kbo_scope(uint32_t team_id)
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
