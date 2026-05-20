#include "../submit_offer_probe/submit_offer_probe.h"

int32_t kbo_no_minor_resolve_current_league_id(void)
{
    KBO_PROFILE_BEGIN(profile_no_minor_resolve_league_id);
    uintptr_t global_db = get_ootp_global_database();
    if (global_db == 0
            || !memory_range_readable(
                (void*)(global_db + OOTP27_GLOBAL_CURRENT_LEAGUE_OFFSET),
                sizeof(uintptr_t))) {
        uint32_t fallback_league_id = kbo_resolve_kbo_league_id();
        int32_t result = fallback_league_id > 0u && fallback_league_id <= 100000u
            ? (int32_t)fallback_league_id
            : 0;
        KBO_PROFILE_END(profile_no_minor_resolve_league_id, "no_minor.resolve_league_id.fallback_global");
        return result;
    }

    uintptr_t current_league_context = *(uintptr_t*)(global_db + OOTP27_GLOBAL_CURRENT_LEAGUE_OFFSET);
    if (current_league_context == 0
            || !memory_range_readable(
                (void*)(current_league_context + OOTP27_GLOBAL_CURRENT_LEAGUE_ID_OFFSET),
                sizeof(int32_t))) {
        uint32_t fallback_league_id = kbo_resolve_kbo_league_id();
        int32_t result = fallback_league_id > 0u && fallback_league_id <= 100000u
            ? (int32_t)fallback_league_id
            : 0;
        KBO_PROFILE_END(profile_no_minor_resolve_league_id, "no_minor.resolve_league_id.fallback_context");
        return result;
    }

    int32_t league_id = *(int32_t*)(current_league_context + OOTP27_GLOBAL_CURRENT_LEAGUE_ID_OFFSET);
    if (league_id <= 0 || league_id > 100000) {
        uint32_t fallback_league_id = kbo_resolve_kbo_league_id();
        int32_t result = fallback_league_id > 0u && fallback_league_id <= 100000u
            ? (int32_t)fallback_league_id
            : 0;
        KBO_PROFILE_END(profile_no_minor_resolve_league_id, "no_minor.resolve_league_id.fallback_invalid");
        return result;
    }
    KBO_PROFILE_END(profile_no_minor_resolve_league_id, "no_minor.resolve_league_id.current_context");
    return league_id;
}

int32_t kbo_no_minor_resolve_current_league_minimum_salary(void)
{
    KBO_PROFILE_BEGIN(profile_no_minor_resolve_min_salary);
    uint8_t* financials = kbo_resolve_current_league_financials(NULL);
    if (financials == NULL
            || !memory_range_readable(
                financials + OOTP27_LEAGUE_MINIMUM_SALARY_OFFSET,
                sizeof(int32_t))) {
        KBO_PROFILE_END(profile_no_minor_resolve_min_salary, "no_minor.resolve_min_salary.no_financials");
        return 0;
    }

    int32_t minimum_salary = *(int32_t*)(financials + OOTP27_LEAGUE_MINIMUM_SALARY_OFFSET);
    if (minimum_salary <= 0 || minimum_salary > kbo_foreign_player_policy()->demand_salary_max) {
        KBO_PROFILE_END(profile_no_minor_resolve_min_salary, "no_minor.resolve_min_salary.invalid");
        return 0;
    }
    KBO_PROFILE_END(profile_no_minor_resolve_min_salary, "no_minor.resolve_min_salary.ok");
    return minimum_salary;
}

int32_t kbo_no_minor_current_league_minimum_salary(void)
{
    return kbo_no_minor_resolve_current_league_minimum_salary();
}

int kbo_no_minor_clamp_player_demand_salary(uintptr_t player_ptr, uintptr_t screen_ptr, const char* source)
{
    KBO_PROFILE_BEGIN(profile_no_minor_clamp_player);
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_demand_floor_enabled, 0, 0) == 0) {
        KBO_PROFILE_END(profile_no_minor_clamp_player, "no_minor.clamp_player.disabled");
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_no_minor_clamp_player_baseline);
    kbo_prepare_foreign_fa_offer_demand_baseline(player_ptr, source);
    kbo_log_financials_salary_baseline_probe(source);
    KBO_PROFILE_END(profile_no_minor_clamp_player_baseline, "no_minor.clamp_player.baseline");
    if (player_ptr == 0 || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        KBO_PROFILE_END(profile_no_minor_clamp_player, "no_minor.clamp_player.bad_player");
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
        : 0u;

    int32_t salary_floor = kbo_no_minor_resolve_current_league_minimum_salary();
    if (salary_floor <= 0) {
        static LONG floor_miss_log_count = 0;
        LONG miss_slot = InterlockedIncrement(&floor_miss_log_count);
        if (miss_slot <= 40) {
            kbo_log_runtimef(
                "KBO no-minor demand floor skipped: source=%s screen=%p player=%u reason=no_floor",
                source,
                (void*)screen_ptr,
                player_id);
        }
        KBO_PROFILE_END(profile_no_minor_clamp_player, "no_minor.clamp_player.no_floor");
        return 0;
    }

    uint32_t league_id = (uint32_t)kbo_no_minor_resolve_current_league_id();
    if (!kbo_no_minor_player_is_teamless_demand_floor_candidate(player, league_id)) {
        KBO_PROFILE_END(profile_no_minor_clamp_player, "no_minor.clamp_player.not_candidate");
        return 0;
    }

    int32_t old_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    uint8_t old_contract_level = *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
    int changed = 0;
    if (old_demand < salary_floor) {
        *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET) = salary_floor;
        changed = 1;
    }

    if (changed) {
        static LONG clamp_log_count = 0;
        LONG clamp_slot = InterlockedIncrement(&clamp_log_count);
        if (clamp_slot <= 120) {
            kbo_log_runtimef(
                "KBO no-minor demand floor applied: source=%s screen=%p player=%u old_demand=%d floor=%d observed_contract_level=%u",
                source,
                (void*)screen_ptr,
                player_id,
                old_demand,
                salary_floor,
                (unsigned)old_contract_level);
        }
    }
    KBO_PROFILE_END(profile_no_minor_clamp_player, changed
        ? "no_minor.clamp_player.changed"
        : "no_minor.clamp_player.unchanged");
    return changed;
}

int kbo_no_minor_clamp_offer_screen_player_salary(uintptr_t screen_ptr, const char* source)
{
    KBO_PROFILE_BEGIN(profile_no_minor_clamp_offer_screen);
    if (screen_ptr == 0 || !memory_range_readable((void*)screen_ptr, 0xc0)) {
        KBO_PROFILE_END(profile_no_minor_clamp_offer_screen, "no_minor.clamp_offer_screen.bad_screen");
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(screen_ptr + OOTP27_FA_OFFER_SCREEN_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        KBO_PROFILE_END(profile_no_minor_clamp_offer_screen, "no_minor.clamp_offer_screen.no_player");
        return 0;
    }

    uint8_t* player = kbo_find_player_by_id(player_id, NULL, NULL);
    if (player == NULL) {
        KBO_PROFILE_END(profile_no_minor_clamp_offer_screen, "no_minor.clamp_offer_screen.player_not_found");
        return 0;
    }

    int changed = kbo_no_minor_clamp_player_demand_salary((uintptr_t)player, screen_ptr, source);
    KBO_PROFILE_END(profile_no_minor_clamp_offer_screen, changed
        ? "no_minor.clamp_offer_screen.changed"
        : "no_minor.clamp_offer_screen.unchanged");
    return changed;
}
