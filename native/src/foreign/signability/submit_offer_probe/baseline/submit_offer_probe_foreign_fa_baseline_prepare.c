#include "../submit_offer_probe.h"
#include "../../../../core/core_flags/keys/runtime_flag_keys.generated.h"

static int kbo_foreign_fa_demand_baseline_enabled(void)
{
    enum { KBO_FOREIGN_FA_DEMAND_BASELINE_FLAG_CACHE_MS = 500u };
    static volatile LONG s_cached_tick = 0;
    static volatile LONG s_cached_enabled = 0;

    DWORD now = GetTickCount();
    LONG cached_tick = InterlockedCompareExchange(&s_cached_tick, 0, 0);
    if (cached_tick != 0
            && (DWORD)(now - (DWORD)cached_tick) <= KBO_FOREIGN_FA_DEMAND_BASELINE_FLAG_CACHE_MS) {
        return InterlockedCompareExchange(&s_cached_enabled, 0, 0) != 0;
    }

    int enabled = read_kbo_localappdata_flag_file(
        KBO_RUNTIME_FLAG_ENABLE_KBO_FOREIGN_FA_DEMAND_BASELINE_FILE);
    InterlockedExchange(&s_cached_enabled, enabled ? 1 : 0);
    InterlockedExchange(&s_cached_tick, (LONG)now);
    return enabled;
}

static int kbo_foreign_fa_player_has_active_reserve_right(
    uint8_t* player,
    uint32_t* out_holder_team_id,
    uint32_t* out_today)
{
    if (out_holder_team_id != NULL) { *out_holder_team_id = 0u; }
    if (out_today != NULL) { *out_today = 0u; }
    if (!kbo_foreign_waiver_ai_enabled()
            || player == NULL
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0u;
    uint32_t holder_team_id = 0u;
    if (player_id == 0u
            || !kbo_get_foreign_waiver_current_yyyymmdd(&today)
            || !kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
            || holder_team_id == 0u) {
        return 0;
    }

    if (out_holder_team_id != NULL) { *out_holder_team_id = holder_team_id; }
    if (out_today != NULL) { *out_today = today; }
    return 1;
}

static int32_t kbo_foreign_fa_reserve_right_baseline_value(int index, int asian_quota)
{
    int32_t base = kbo_get_foreign_fa_demand_baseline_value_for_player(index, asian_quota);
    int32_t floor = kbo_get_foreign_fa_demand_baseline_value_for_player(0, asian_quota);
    int64_t discounted = ((int64_t)base * kbo_foreign_player_policy()->reserve_demand_discount_percent) / 100;
    if (discounted < floor) {
        discounted = floor;
    }
    if (discounted > INT32_MAX) {
        discounted = INT32_MAX;
    }
    return (int32_t)discounted;
}

__declspec(noinline) void ootp_kbo_foreign_fa_demand_baseline_prepare_wrapper(
    uintptr_t financials_ptr,
    uintptr_t player_ptr,
    uint32_t source_rva)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    kbo_restore_foreign_fa_demand_salary_ladder("prepare_enter");
    if (!kbo_foreign_fa_demand_baseline_enabled()) {
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_demand_baseline_prepare");
    }
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) != 0) {
        static LONG pending_restore_log_count = 0;
        LONG pending_slot = InterlockedIncrement(&pending_restore_log_count);
        if (pending_slot <= 40) {
            kbo_log_runtimef(
                "KBO foreign FA demand baseline prepare skipped source=0x%x reason=restore_pending",
                source_rva);
        }
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_demand_baseline_prepare");
    }

    if (financials_ptr == 0
            || !memory_range_readable(
                (void*)(financials_ptr + OOTP27_FINANCIALS_AVERAGE_SALARY_OFFSET),
                sizeof(int32_t))
            || !memory_range_readable(
                (void*)(financials_ptr + OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET),
                sizeof(int32_t))) {
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_demand_baseline_prepare");
    }
    if (!kbo_player_pointer_plausible(player_ptr)) {
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_demand_baseline_prepare");
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_demand_baseline_prepare");
    }
    uint32_t player_id = memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
        ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
        : 0u;
    int asian_quota = kbo_player_is_asian_quota_slot_candidate(player);
    uint32_t reserve_holder_team_id = 0u;
    uint32_t reserve_today = 0u;
    int reserve_right = kbo_foreign_fa_player_has_active_reserve_right(
        player,
        &reserve_holder_team_id,
        &reserve_today);

    uint8_t* financials = (uint8_t*)financials_ptr;
    for (int i = 0; i < 9; i++) {
        if (!memory_range_readable(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i], sizeof(int32_t))) {
            KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_demand_baseline_prepare");
        }
    }

    KboFinancialSalaryLadderSnapshot prepared_snapshot = {0};
    int patched = 0;
    kbo_lock_enter(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);
    memset(&g_kbo_foreign_fa_demand_ladder_snapshot, 0, sizeof(g_kbo_foreign_fa_demand_ladder_snapshot));
    for (int i = 0; i < 9; i++) {
        g_kbo_foreign_fa_demand_ladder_snapshot.values[i] =
            *(int32_t*)(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i]);
    }
    g_kbo_foreign_fa_demand_ladder_snapshot.demand_ceiling_value =
        *(int32_t*)(financials + OOTP27_FINANCIALS_FA_DEMAND_CEILING_OFFSET);
    g_kbo_foreign_fa_demand_ladder_snapshot.financials = financials;
    g_kbo_foreign_fa_demand_ladder_snapshot.player_id = player_id;
    g_kbo_foreign_fa_demand_ladder_snapshot.source_rva = source_rva;
    g_kbo_foreign_fa_demand_ladder_snapshot.asian_quota = (uint32_t)asian_quota;
    g_kbo_foreign_fa_demand_ladder_snapshot.reserve_right = (uint32_t)reserve_right;
    g_kbo_foreign_fa_demand_ladder_snapshot.holder_team_id = reserve_holder_team_id;
    g_kbo_foreign_fa_demand_ladder_snapshot.today = reserve_today;
    g_kbo_foreign_fa_demand_ladder_snapshot.generation =
        InterlockedIncrement(&g_kbo_foreign_fa_demand_ladder_snapshot_generation);

    for (int i = 0; i < 9; i++) {
        int32_t patched_value = reserve_right
            ? kbo_foreign_fa_reserve_right_baseline_value(i, asian_quota)
            : kbo_get_foreign_fa_demand_baseline_value_for_player(i, asian_quota);
        g_kbo_foreign_fa_demand_ladder_snapshot.patched_values[i] = patched_value;
    }
    g_kbo_foreign_fa_demand_ladder_snapshot.patched_demand_ceiling_value =
        reserve_right
            ? kbo_foreign_fa_reserve_right_baseline_value(8, asian_quota)
            : kbo_get_foreign_fa_demand_baseline_value_for_player(8, asian_quota);
    patched = kbo_write_foreign_fa_financials_values(
        financials,
        g_kbo_foreign_fa_demand_ladder_snapshot.patched_values,
        g_kbo_foreign_fa_demand_ladder_snapshot.patched_demand_ceiling_value);

    InterlockedExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 1);
    prepared_snapshot = g_kbo_foreign_fa_demand_ladder_snapshot;
    kbo_lock_leave(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);

    if (patched != 10) {
        kbo_restore_foreign_fa_demand_salary_ladder("prepare_failed");
        KBO_HOOK_PROFILE_RETURN_VOID(profile_hook, "foreign.fa_demand_baseline_prepare");
    }

    static LONG prepare_log_count = 0;
    LONG slot = InterlockedIncrement(&prepare_log_count);
    if (slot <= 120) {
        kbo_log_runtimef(
            "KBO foreign FA demand baseline prepared source=0x%x generation=%ld player=%u asian_quota=%d reserve_right=%d holder_team=%u today=%u financials=%p patched=%d original_min=%d original_superstar=%d original_ceiling=%d foreign_min=%d foreign_superstar=%d foreign_ceiling=%d",
            source_rva,
            prepared_snapshot.generation,
            player_id,
            asian_quota,
            reserve_right,
            reserve_holder_team_id,
            reserve_today,
            (void*)financials,
            patched,
            prepared_snapshot.values[0],
            prepared_snapshot.values[8],
            prepared_snapshot.demand_ceiling_value,
            prepared_snapshot.patched_values[0],
            prepared_snapshot.patched_values[8],
            prepared_snapshot.patched_demand_ceiling_value);
    }
    KBO_HOOK_PROFILE_END(profile_hook, "foreign.fa_demand_baseline_prepare");
}

static uint32_t kbo_foreign_fa_offer_baseline_source_rva(const char* source)
{
    uint32_t source_rva = 0u;
    if (source != NULL && strstr(source, "17B50B4") != NULL) {
        kbo_resolve_build_specific_rva(
            OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17B50B4_RVA,
            &source_rva);
    } else if (source != NULL && strstr(source, "17A79BB") != NULL) {
        kbo_resolve_build_specific_rva(
            OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17A79BB_RVA,
            &source_rva);
    } else if (source != NULL && strstr(source, "foreign_ai_offer_terms") != NULL) {
        kbo_resolve_build_specific_rva(
            OOTP27_AI_FA_OFFER_TERMS_BUILD_PREP_RVA,
            &source_rva);
    } else if (source != NULL && strstr(source, "foreign_ai_offer_build") != NULL) {
        kbo_resolve_build_specific_rva(
            OOTP27_AI_FA_OFFER_BUILD_PREP_RVA,
            &source_rva);
    } else if (source != NULL && strstr(source, "foreign_ai_offer_attach") != NULL) {
        kbo_resolve_build_specific_rva(
            OOTP27_PLAYER_CONTRACT_OFFER_ATTACH_RVA,
            &source_rva);
    }
    return source_rva;
}

static void kbo_prepare_foreign_fa_offer_demand_baseline_with_financials(
    uintptr_t player_ptr,
    const char* source,
    uint8_t* financials,
    uint32_t league_id,
    int32_t team_key)
{
    if (financials == NULL) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t source_rva = kbo_foreign_fa_offer_baseline_source_rva(source);
    ootp_kbo_foreign_fa_demand_baseline_prepare_wrapper((uintptr_t)financials, player_ptr, source_rva);
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) != 0) {
        kbo_schedule_foreign_fa_demand_restore_timer();
        static LONG offer_prepare_log_count = 0;
        LONG slot = InterlockedIncrement(&offer_prepare_log_count);
        if (slot <= 120) {
            kbo_log_runtimef(
                "KBO foreign FA demand baseline offer-build active source=%s restore_timer=1 player=%u team_key=%d league=%u financials=%p",
                source != NULL ? source : "",
                memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
                    ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
                    : 0u,
                team_key,
                league_id,
                (void*)financials);
        }
    }
}

void kbo_prepare_foreign_fa_offer_demand_baseline(uintptr_t player_ptr, const char* source)
{
    if (!kbo_foreign_fa_demand_baseline_enabled()) {
        kbo_restore_foreign_fa_demand_salary_ladder("offer_baseline_disabled");
        return;
    }
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    uint32_t league_id = 0u;
    uint8_t* financials = kbo_resolve_current_league_financials(&league_id);
    if (financials == NULL) {
        static LONG no_financials_log_count = 0;
        LONG slot = InterlockedIncrement(&no_financials_log_count);
        if (slot <= 40) {
            kbo_log_runtimef(
                "KBO foreign FA demand baseline offer-build skipped source=%s reason=no_financials player=%u",
                source != NULL ? source : "",
                memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
                    ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
                    : 0u);
        }
        return;
    }

    kbo_prepare_foreign_fa_offer_demand_baseline_with_financials(
        player_ptr,
        source,
        financials,
        league_id,
        0);
}

void kbo_prepare_foreign_fa_offer_demand_baseline_for_team_key(
    uintptr_t player_ptr,
    int32_t team_key,
    const char* source)
{
    if (!kbo_foreign_fa_demand_baseline_enabled()) {
        kbo_restore_foreign_fa_demand_salary_ladder("offer_baseline_disabled");
        return;
    }
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    uint32_t league_id = 0u;
    uint8_t* financials = kbo_resolve_team_key_league_financials(team_key, &league_id);
    if (financials == NULL) {
        static LONG no_team_financials_log_count = 0;
        LONG slot = InterlockedIncrement(&no_team_financials_log_count);
        if (slot <= 80) {
            kbo_log_runtimef(
                "KBO foreign FA demand baseline offer-build target financials skipped source=%s reason=no_team_financials player=%u team_key=%d",
                source != NULL ? source : "",
                memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
                    ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
                    : 0u,
                team_key);
        }
        kbo_prepare_foreign_fa_offer_demand_baseline(player_ptr, source);
        return;
    }

    kbo_prepare_foreign_fa_offer_demand_baseline_with_financials(
        player_ptr,
        source,
        financials,
        league_id,
        team_key);
}

uint8_t* kbo_resolve_current_league_financials(uint32_t* out_league_id)
{
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }

    int32_t league_id = kbo_no_minor_resolve_current_league_id();
    if (league_id <= 0) {
        return NULL;
    }

    uintptr_t global_db = get_ootp_global_database();
    if (global_db == 0) {
        return NULL;
    }
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    OotpLeagueFinancialsLookupFn lookup =
        (OotpLeagueFinancialsLookupFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA);
    if (!memory_range_readable((void*)lookup, 16)) {
        return NULL;
    }

    uint8_t* financials = lookup((void*)global_db, league_id);
    if (financials == NULL
            || !memory_range_readable(financials + OOTP27_FINANCIALS_PROBE_LAST_OFFSET, sizeof(int32_t))) {
        return NULL;
    }

    if (out_league_id != NULL) {
        *out_league_id = (uint32_t)league_id;
    }
    return financials;
}

uint8_t* kbo_resolve_team_key_league_financials(int32_t team_key, uint32_t* out_league_id)
{
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }
    if (team_key <= 0) {
        return NULL;
    }

    uintptr_t global_db = get_ootp_global_database();
    if (global_db == 0
            || !memory_range_readable((void*)(global_db + OOTP27_KBO_TEAM_COUNT_OFFSET), sizeof(int32_t))
            || !memory_range_readable((void*)(global_db + OOTP27_KBO_TEAM_VECTOR_OFFSET), sizeof(uintptr_t))) {
        return NULL;
    }

    int32_t team_count = *(int32_t*)(global_db + OOTP27_KBO_TEAM_COUNT_OFFSET);
    int32_t team_index = team_key - 1;
    if (team_count <= 0 || team_count > 100000 || team_index < 0 || team_index >= team_count) {
        return NULL;
    }

    uintptr_t team_vector = *(uintptr_t*)(global_db + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    uintptr_t team_slot = team_vector + ((uintptr_t)team_index * sizeof(uintptr_t));
    if (team_vector == 0 || !memory_range_readable((void*)team_slot, sizeof(uintptr_t))) {
        return NULL;
    }

    uint8_t* team = *(uint8_t**)team_slot;
    if (team == NULL
            || !memory_range_readable(
                team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET,
                sizeof(int32_t))) {
        return NULL;
    }

    int32_t league_id = *(int32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (league_id <= 0) {
        return NULL;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    OotpLeagueFinancialsLookupFn lookup =
        (OotpLeagueFinancialsLookupFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA);
    if (!memory_range_readable((void*)lookup, 16)) {
        return NULL;
    }

    uint8_t* financials = lookup((void*)global_db, league_id);
    if (financials == NULL
            || !memory_range_readable(financials + OOTP27_FINANCIALS_PROBE_LAST_OFFSET, sizeof(int32_t))) {
        return NULL;
    }

    uint32_t effective_league_id = (uint32_t)league_id;
    if (memory_range_readable(
            financials + OOTP27_KBO_LEAGUE_FINANCIALS_REDIRECT_LEAGUE_ID_OFFSET,
            sizeof(int32_t))) {
        int32_t redirect_league_id =
            *(int32_t*)(financials + OOTP27_KBO_LEAGUE_FINANCIALS_REDIRECT_LEAGUE_ID_OFFSET);
        if (redirect_league_id > 0 && redirect_league_id != league_id) {
            uint8_t* redirected_financials = lookup((void*)global_db, redirect_league_id);
            if (redirected_financials == NULL
                    || !memory_range_readable(
                        redirected_financials + OOTP27_FINANCIALS_PROBE_LAST_OFFSET,
                        sizeof(int32_t))) {
                return NULL;
            }
            financials = redirected_financials;
            effective_league_id = (uint32_t)redirect_league_id;
        }
    }

    if (out_league_id != NULL) {
        *out_league_id = effective_league_id;
    }
    return financials;
}

void kbo_log_financials_salary_baseline_probe(const char* source)
{
#define KBO_FINANCIALS_PROBE_LOW_OFFSET(index) \
    (OOTP27_FINANCIALS_PROBE_FIRST_OFFSET + ((uint32_t)(index) * sizeof(int32_t)))
#define KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(index) \
    (OOTP27_FINANCIALS_PROBE_SECONDARY_FIRST_OFFSET + ((uint32_t)(index) * sizeof(int32_t)))

    static LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 5 && !read_kbo_localappdata_flag_file(KBO_RUNTIME_FLAG_ENABLE_KBO_FINANCIALS_SALARY_BASELINE_PROBE_LOG_FILE)) {
        return;
    }

    uint32_t league_id = 0u;
    uint8_t* financials = kbo_resolve_current_league_financials(&league_id);
    if (financials == NULL) {
        if (slot <= 20) {
            kbo_log_runtimef("KBO financials salary baseline probe skipped source=%s reason=no_financials", source != NULL ? source : "");
        }
        return;
    }

    kbo_log_runtimef(
        "KBO financials salary baseline probe source=%s league=%u financials=%p off278=%d off27c=%d off280=%d off284=%d off288=%d off28c=%d off290=%d off294=%d off298=%d off29c=%d off2a0=%d off2a4=%d off2a8=%d off2ac=%d off360=%d off364=%d off368=%d off36c=%d off370=%d off374=%d off378=%d off37c=%d off380=%d off384=%d off388=%d off38c=%d off390=%d",
        source != NULL ? source : "",
        league_id,
        (void*)financials,
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(0)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(1)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(2)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(3)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(4)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(5)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(6)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(7)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(8)),
        *(int32_t*)(financials + OOTP27_FINANCIALS_AVERAGE_SALARY_OFFSET),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(10)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(11)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(12)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(13)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(0)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(1)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(2)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(3)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(4)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(5)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(6)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(7)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(8)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(9)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(10)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(11)),
        *(int32_t*)(financials + OOTP27_FINANCIALS_PROBE_LAST_OFFSET));

#undef KBO_FINANCIALS_PROBE_LOW_OFFSET
#undef KBO_FINANCIALS_PROBE_SECONDARY_OFFSET
}

