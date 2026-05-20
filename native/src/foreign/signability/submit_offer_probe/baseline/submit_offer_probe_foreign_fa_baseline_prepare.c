#include "../submit_offer_probe.h"
#include "../../../../core/core_flags/keys/runtime_flag_keys.generated.h"

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

    if (financials_ptr == 0
            || !memory_range_readable(
                (void*)(financials_ptr + OOTP27_FINANCIALS_AVERAGE_SALARY_OFFSET),
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
    int asian_quota = kbo_player_is_asian_quota_slot_candidate(player);
    uint32_t reserve_holder_team_id = 0u;
    uint32_t reserve_today = 0u;
    int reserve_right = kbo_foreign_fa_player_has_active_reserve_right(
        player,
        &reserve_holder_team_id,
        &reserve_today);

    uint8_t* financials = (uint8_t*)financials_ptr;
    for (int i = 0; i < 9; i++) {
        g_kbo_foreign_fa_demand_ladder_snapshot.values[i] =
            *(int32_t*)(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i]);
    }
    g_kbo_foreign_fa_demand_ladder_snapshot.financials = financials;

    int patched = 0;
    for (int i = 0; i < 9; i++) {
        patched += kbo_write_i32(
            financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i],
            reserve_right
                ? kbo_foreign_fa_reserve_right_baseline_value(i, asian_quota)
                : kbo_get_foreign_fa_demand_baseline_value_for_player(i, asian_quota));
    }

    if (patched == 9) {
        InterlockedExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 1);
    } else {
        kbo_restore_foreign_fa_demand_salary_ladder("prepare_failed");
    }

    static LONG prepare_log_count = 0;
    LONG slot = InterlockedIncrement(&prepare_log_count);
    if (slot <= 120) {
        kbo_log_runtimef(
            "KBO foreign FA demand baseline prepared source=0x%x player=%u asian_quota=%d reserve_right=%d holder_team=%u today=%u financials=%p patched=%d original_superstar=%d foreign_superstar=%d",
            source_rva,
            memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
                ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
                : 0u,
            asian_quota,
            reserve_right,
            reserve_holder_team_id,
            reserve_today,
            (void*)financials,
            patched,
            g_kbo_foreign_fa_demand_ladder_snapshot.values[8],
            reserve_right
                ? kbo_foreign_fa_reserve_right_baseline_value(8, asian_quota)
                : kbo_get_foreign_fa_demand_baseline_value_for_player(8, asian_quota));
    }
    KBO_HOOK_PROFILE_END(profile_hook, "foreign.fa_demand_baseline_prepare");
}

void kbo_prepare_foreign_fa_offer_demand_baseline(uintptr_t player_ptr, const char* source)
{
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }
    kbo_apply_foreign_contract_demand_floor(player_ptr, 0u, source != NULL ? source : "offer_demand_baseline_prepare");

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

    uint32_t source_rva = 0u;
    if (source != NULL && strstr(source, "17B50B4") != NULL) {
        source_rva = OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17B50B4_RVA;
    } else if (source != NULL && strstr(source, "17A79BB") != NULL) {
        source_rva = OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17A79BB_RVA;
    }

    ootp_kbo_foreign_fa_demand_baseline_prepare_wrapper((uintptr_t)financials, player_ptr, source_rva);
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) != 0) {
        kbo_schedule_foreign_fa_demand_restore_timer();
        static LONG offer_prepare_log_count = 0;
        LONG slot = InterlockedIncrement(&offer_prepare_log_count);
        if (slot <= 120) {
            kbo_log_runtimef(
                "KBO foreign FA demand baseline offer-build active source=%s player=%u league=%u financials=%p",
                source != NULL ? source : "",
                memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
                    ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
                    : 0u,
                league_id,
                (void*)financials);
        }
    }
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

