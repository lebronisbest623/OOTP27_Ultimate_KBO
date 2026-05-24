#include "submit_offer_probe_no_minor_demand_internal.h"

volatile LONG g_kbo_no_minor_contract_demand_floor_scanner_started = 0;

int kbo_no_minor_copy_player_scan_prefix(uintptr_t player_ptr, uint8_t* out, SIZE_T out_size)
{
    if (player_ptr == 0 || out == NULL || out_size < KBO_NO_MINOR_SCAN_PREFILTER_BYTES) {
        return 0;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_ptr,
            out,
            KBO_NO_MINOR_SCAN_PREFILTER_BYTES,
            &bytes_read)
            || bytes_read != KBO_NO_MINOR_SCAN_PREFILTER_BYTES) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(out + OOTP27_PLAYER_ID_OFFSET);
    uint16_t age = *(uint16_t*)(out + OOTP27_PLAYER_AGE_OFFSET);
    return kbo_foreign_policy_player_id_plausible(player_id)
        && kbo_foreign_policy_player_copy_age_plausible(age);
}

int kbo_no_minor_copy_player_scan(uintptr_t player_ptr, uint8_t* out)
{
    if (player_ptr == 0 || out == NULL) {
        return 0;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_ptr,
            out,
            OOTP27_PLAYER_SCAN_BYTES,
            &bytes_read)
            || bytes_read != OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(out + OOTP27_PLAYER_ID_OFFSET);
    uint16_t age = *(uint16_t*)(out + OOTP27_PLAYER_AGE_OFFSET);
    return kbo_foreign_policy_player_id_plausible(player_id)
        && kbo_foreign_policy_player_copy_age_plausible(age);
}

int kbo_no_minor_read_player_i32(uintptr_t player_ptr, uint32_t offset, int32_t* out)
{
    if (player_ptr == 0 || out == NULL || offset + sizeof(int32_t) > OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }

    SIZE_T bytes_read = 0;
    return ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)(player_ptr + offset),
            out,
            sizeof(*out),
            &bytes_read)
        && bytes_read == sizeof(*out);
}

int kbo_no_minor_write_player_i32(uintptr_t player_ptr, uint32_t offset, int32_t value)
{
    if (player_ptr == 0 || offset + sizeof(int32_t) > OOTP27_PLAYER_SCAN_BYTES) {
        return 0;
    }
    return kbo_write_i32((uint8_t*)(player_ptr + offset), value);
}

static int kbo_no_minor_scan_has_nonzero_evaluation(const uint8_t* scan)
{
    if (scan == NULL) {
        return 0;
    }

    int16_t overall = *(int16_t*)(scan + OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int16_t talent = *(int16_t*)(scan + OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int16_t ratings = *(int16_t*)(scan + OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int16_t career = *(int16_t*)(scan + OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    return overall > 0 || talent > 0 || ratings > 0 || career > 0;
}

int kbo_no_minor_scan_is_teamless_demand_floor_candidate(const uint8_t* scan, uint32_t league_id)
{
    if (scan == NULL) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(scan + OOTP27_PLAYER_ID_OFFSET);
    uint16_t age = *(uint16_t*)(scan + OOTP27_PLAYER_AGE_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(scan + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    if (!kbo_foreign_policy_player_id_plausible(player_id)
            || !kbo_foreign_policy_market_age_allowed(age)
            || current_team_id != 0u) {
        return 0;
    }
    if (scan[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
            || scan[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET] != 0u) {
        return 0;
    }

    uint32_t current_league_id = *(uint32_t*)(scan + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t draft_league_id = *(uint32_t*)(scan + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    if (league_id != 0u
            && current_league_id != league_id
            && draft_league_id != league_id
            && !kbo_no_minor_scan_has_nonzero_evaluation(scan)) {
        return 0;
    }

    return 1;
}

int kbo_no_minor_scan_is_foreign_demand_remap_candidate(const uint8_t* scan)
{
    if (scan == NULL) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(scan + OOTP27_PLAYER_ID_OFFSET);
    uint16_t age = *(uint16_t*)(scan + OOTP27_PLAYER_AGE_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(scan + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(scan + OOTP27_PLAYER_NATION_ID_OFFSET);
    int32_t demand = *(int32_t*)(scan + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    if (!kbo_foreign_policy_player_id_plausible(player_id)
            || !kbo_foreign_policy_market_age_allowed(age)
            || current_team_id != 0u) {
        return 0;
    }
    if (scan[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
        return 0;
    }
    if (nation_id == 0u || nation_id == OOTP27_KBO_KOREA_NATION_ID) {
        return 0;
    }
    return kbo_foreign_policy_demand_salary_plausible(demand);
}

uintptr_t* kbo_no_minor_copy_player_vector_snapshot(
    uintptr_t player_vector,
    int32_t player_count,
    const char** out_failure_reason)
{
    if (out_failure_reason != NULL) {
        *out_failure_reason = "unknown";
    }
    if (player_vector == 0 || player_count <= 0) {
        if (out_failure_reason != NULL) { *out_failure_reason = "invalid_vector"; }
        return NULL;
    }
    if ((SIZE_T)player_count > ((SIZE_T)-1 / sizeof(uintptr_t))) {
        if (out_failure_reason != NULL) { *out_failure_reason = "count_overflow"; }
        return NULL;
    }

    SIZE_T player_vector_bytes = (SIZE_T)player_count * sizeof(uintptr_t);
    if (!memory_range_readable((void*)player_vector, player_vector_bytes)) {
        if (out_failure_reason != NULL) { *out_failure_reason = "unreadable_vector"; }
        return NULL;
    }

    uintptr_t* snapshot = (uintptr_t*)HeapAlloc(GetProcessHeap(), 0, player_vector_bytes);
    if (snapshot == NULL) {
        if (out_failure_reason != NULL) { *out_failure_reason = "alloc_failed"; }
        return NULL;
    }

    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (LPCVOID)player_vector,
            snapshot,
            player_vector_bytes,
            &bytes_read)
            || bytes_read != player_vector_bytes) {
        HeapFree(GetProcessHeap(), 0, snapshot);
        if (out_failure_reason != NULL) { *out_failure_reason = "copy_failed"; }
        return NULL;
    }

    if (out_failure_reason != NULL) {
        *out_failure_reason = NULL;
    }
    return snapshot;
}

int kbo_no_minor_player_is_teamless_demand_floor_candidate(uint8_t* player, uint32_t league_id)
{
    uint8_t scan[OOTP27_PLAYER_SCAN_BYTES] = {0};
    if (!kbo_no_minor_copy_player_scan((uintptr_t)player, scan)) {
        return 0;
    }

    return kbo_no_minor_scan_is_teamless_demand_floor_candidate(scan, league_id);
}

__declspec(noinline) int32_t ootp_kbo_no_minor_demand_write_floor_probe(
    uintptr_t player_ptr,
    int32_t proposed_demand,
    uint32_t source_rva,
    int32_t salary_floor_hint)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    KBO_PROFILE_BEGIN(profile_no_minor_write_probe);
    if (kbo_runtime_save_in_progress()) {
        KBO_PROFILE_END(profile_no_minor_write_probe, "no_minor.write_probe.save_in_progress");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.no_minor_demand_write_floor", proposed_demand);
    }
    KboFinancialSalaryLadderSnapshot baseline_snapshot = {0};
    int baseline_active_on_entry = 0;
    kbo_lock_enter(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);
    baseline_active_on_entry =
        InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) != 0;
    if (baseline_active_on_entry) {
        baseline_snapshot = g_kbo_foreign_fa_demand_ladder_snapshot;
    }
    kbo_lock_leave(&g_kbo_foreign_fa_demand_ladder_snapshot_lock);
    uint32_t baseline_player_id = baseline_active_on_entry ? baseline_snapshot.player_id : 0u;
    uint32_t baseline_source_rva = baseline_active_on_entry ? baseline_snapshot.source_rva : 0u;
    uint32_t baseline_asian_quota = baseline_active_on_entry ? baseline_snapshot.asian_quota : 0u;
    uint32_t baseline_reserve_right = baseline_active_on_entry ? baseline_snapshot.reserve_right : 0u;
    int32_t baseline_original_min = baseline_active_on_entry ? baseline_snapshot.values[0] : 0;
    int32_t baseline_original_superstar = baseline_active_on_entry ? baseline_snapshot.values[8] : 0;
    int32_t baseline_original_ceiling = baseline_active_on_entry ? baseline_snapshot.demand_ceiling_value : 0;
    int32_t baseline_patched_min = baseline_active_on_entry ? baseline_snapshot.patched_values[0] : 0;
    int32_t baseline_patched_superstar = baseline_active_on_entry ? baseline_snapshot.patched_values[8] : 0;
    int32_t baseline_patched_ceiling = baseline_active_on_entry ? baseline_snapshot.patched_demand_ceiling_value : 0;
    uintptr_t baseline_financials = baseline_active_on_entry ? (uintptr_t)baseline_snapshot.financials : 0u;
    kbo_restore_foreign_fa_demand_salary_ladder("demand_write");
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_demand_floor_enabled, 0, 0) == 0) {
        KBO_PROFILE_END(profile_no_minor_write_probe, "no_minor.write_probe.disabled");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.no_minor_demand_write_floor", proposed_demand);
    }
    KBO_PROFILE_BEGIN(profile_no_minor_write_probe_baseline);
    kbo_log_financials_salary_baseline_probe("demand_write");
    KBO_PROFILE_END(profile_no_minor_write_probe_baseline, "no_minor.write_probe.log_baseline");
    if (!kbo_player_pointer_plausible(player_ptr)) {
        KBO_PROFILE_END(profile_no_minor_write_probe, "no_minor.write_probe.bad_player");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.no_minor_demand_write_floor", proposed_demand);
    }

    uint8_t* player = (uint8_t*)player_ptr;
    KBO_PROFILE_BEGIN(profile_no_minor_write_probe_league);
    uint32_t league_id = (uint32_t)kbo_no_minor_resolve_current_league_id();
    KBO_PROFILE_END(profile_no_minor_write_probe_league, "no_minor.write_probe.resolve_league");
    KBO_PROFILE_BEGIN(profile_no_minor_write_probe_candidate);
    if (!kbo_no_minor_player_is_teamless_demand_floor_candidate(player, league_id)) {
        KBO_PROFILE_END(profile_no_minor_write_probe_candidate, "no_minor.write_probe.candidate_check");
        KBO_PROFILE_END(profile_no_minor_write_probe, "no_minor.write_probe.not_candidate");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.no_minor_demand_write_floor", proposed_demand);
    }
    KBO_PROFILE_END(profile_no_minor_write_probe_candidate, "no_minor.write_probe.candidate_check");

    int32_t salary_floor = salary_floor_hint;
    if (salary_floor <= 0 || salary_floor > kbo_foreign_player_policy()->demand_salary_max) {
        KBO_PROFILE_BEGIN(profile_no_minor_write_probe_floor);
        salary_floor = kbo_no_minor_resolve_current_league_minimum_salary();
        KBO_PROFILE_END(profile_no_minor_write_probe_floor, "no_minor.write_probe.resolve_floor");
    }
    if (salary_floor <= 0) {
        KBO_PROFILE_END(profile_no_minor_write_probe, "no_minor.write_probe.no_floor");
        KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.no_minor_demand_write_floor", proposed_demand);
    }

    uint8_t scan[OOTP27_PLAYER_SCAN_BYTES] = {0};
    int scan_available = kbo_no_minor_copy_player_scan((uintptr_t)player, scan);
    uint32_t player_id = scan_available ? *(uint32_t*)(scan + OOTP27_PLAYER_ID_OFFSET) : 0u;
    uint8_t observed_contract_level = scan_available ? scan[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET] : 0u;
    uint32_t nation_id = scan_available ? *(uint32_t*)(scan + OOTP27_PLAYER_NATION_ID_OFFSET) : 0u;
    uint32_t current_team_id = scan_available ? *(uint32_t*)(scan + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) : 0u;
    int16_t overall = scan_available ? *(int16_t*)(scan + OOTP27_PLAYER_OVERALL_VALUE_OFFSET) : 0;
    int16_t ratings = scan_available ? *(int16_t*)(scan + OOTP27_PLAYER_RATINGS_VALUE_OFFSET) : 0;
    int16_t career = scan_available ? *(int16_t*)(scan + OOTP27_PLAYER_CAREER_VALUE_OFFSET) : 0;

    int32_t adjusted_demand = proposed_demand < salary_floor ? salary_floor : proposed_demand;
    if (scan_available && kbo_player_is_foreign_for_kbo_rights(player)) {
        static LONG foreign_actual_log_count = 0;
        LONG foreign_slot = InterlockedIncrement(&foreign_actual_log_count);
        if (foreign_slot <= 240) {
            int asian_quota = kbo_player_is_asian_quota_slot_candidate(player) ? 1 : 0;
            uint32_t today = 0u;
            if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
                today = 0u;
            }
            int32_t current_demand = memory_range_readable(
                    player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET,
                    sizeof(int32_t))
                ? *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET)
                : 0;
            uint32_t baseline_match = baseline_active_on_entry
                && baseline_player_id != 0u
                && baseline_player_id == player_id;
            kbo_log_runtimef(
                "KBO foreign FA actual demand write observed: source=0x%x player=%u nation=%u asian_quota=%d proposed=%d adjusted=%d current=%d minimum=%d baseline_min=%d baseline_superstar=%d level=%u today=%u ovr=%d rat=%d car=%d baseline_active=%d baseline_player=%u baseline_match=%u baseline_source=0x%x baseline_asian=%u baseline_reserve=%u baseline_original_min=%d baseline_original_superstar=%d baseline_original_ceiling=%d baseline_patched_min=%d baseline_patched_superstar=%d baseline_patched_ceiling=%d baseline_financials=%p",
                source_rva,
                player_id,
                nation_id,
                asian_quota,
                proposed_demand,
                adjusted_demand,
                current_demand,
                salary_floor,
                kbo_get_foreign_fa_demand_baseline_value_for_player(0, asian_quota),
                kbo_get_foreign_fa_demand_baseline_value_for_player(8, asian_quota),
                (unsigned)observed_contract_level,
                today,
                (int)overall,
                (int)ratings,
                (int)career,
                baseline_active_on_entry,
                baseline_player_id,
                baseline_match,
                baseline_source_rva,
                baseline_asian_quota,
                baseline_reserve_right,
                baseline_original_min,
                baseline_original_superstar,
                baseline_original_ceiling,
                baseline_patched_min,
                baseline_patched_superstar,
                baseline_patched_ceiling,
                (void*)baseline_financials);
        }
    }
    if (scan_available
            && nation_id == OOTP27_KBO_KOREA_NATION_ID
            && current_team_id == 0u
            && proposed_demand <= salary_floor + 1000) {
        static LONG domestic_actual_log_count = 0;
        LONG actual_slot = InterlockedIncrement(&domestic_actual_log_count);
        if (actual_slot <= 240) {
            kbo_log_runtimef(
                "KBO domestic FA actual demand write observed: source=0x%x player=%u proposed=%d adjusted=%d minimum=%d level=%u ovr=%d rat=%d car=%d",
                source_rva,
                player_id,
                proposed_demand,
                adjusted_demand,
                salary_floor,
                (unsigned)observed_contract_level,
                (int)overall,
                (int)ratings,
                (int)career);
        }
    }

    if (adjusted_demand != proposed_demand) {
        static LONG write_floor_log_count = 0;
        LONG slot = InterlockedIncrement(&write_floor_log_count);
        if (slot <= 120) {
            kbo_log_runtimef(
                "KBO no-minor demand write floor: source=0x%x player=%u proposed=%d adjusted=%d floor=%d observed_contract_level=%u",
                source_rva,
                player_id,
                proposed_demand,
                adjusted_demand,
                salary_floor,
                (unsigned)observed_contract_level);
        }
    }

    KBO_PROFILE_END(profile_no_minor_write_probe, adjusted_demand != proposed_demand
        ? "no_minor.write_probe.adjusted"
        : "no_minor.write_probe.unchanged");
    KBO_HOOK_PROFILE_RETURN(profile_hook, "foreign.no_minor_demand_write_floor", adjusted_demand);
}

DWORD WINAPI kbo_no_minor_contract_demand_floor_scanner_thread(LPVOID param)
{
    (void)param;
    const KboForeignPlayerPolicy* policy = kbo_foreign_player_policy();
    if (!kbo_runtime_sleep_should_continue((uint32_t)policy->no_minor_scan_initial_delay_ms)) {
        InterlockedExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 0);
        kbo_log_runtime_line("stopped KBO no-minor demand floor scanner thread");
        return 0;
    }

    for (uint32_t attempt = 0; kbo_runtime_threads_should_continue(); attempt++) {
        KBO_PROFILE_BEGIN(profile_no_minor_scanner_tick);
        if (!kbo_runtime_pause_for_save_if_needed("no_minor_demand_floor_scanner")) {
            KBO_PROFILE_END(profile_no_minor_scanner_tick, "no_minor.scanner_tick.save_stop");
            break;
        }
        kbo_no_minor_scan_and_floor_teamless_fa_demands("background_prescan");
        KBO_PROFILE_END(profile_no_minor_scanner_tick, attempt < (uint32_t)policy->no_minor_scan_warmup_attempts
            ? "no_minor.scanner_tick.warmup"
            : "no_minor.scanner_tick.steady");
        if (!kbo_runtime_sleep_should_continue(attempt < (uint32_t)policy->no_minor_scan_warmup_attempts
            ? (uint32_t)policy->no_minor_scan_warmup_interval_ms
            : (uint32_t)policy->no_minor_scan_interval_ms)) {
            break;
        }
    }
    InterlockedExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 0);
    kbo_log_runtime_line("stopped KBO no-minor demand floor scanner thread");
    return 0;
}

void start_kbo_no_minor_contract_demand_floor_scanner_thread(void)
{
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 1, 0) != 0) {
        return;
    }

    if (!kbo_start_runtime_thread(
            kbo_no_minor_contract_demand_floor_scanner_thread,
            NULL,
            "no-minor demand floor scanner")) {
        InterlockedExchange(&g_kbo_no_minor_contract_demand_floor_scanner_started, 0);
        return;
    }
    kbo_log_runtime_line("started KBO no-minor demand floor scanner thread");
}

