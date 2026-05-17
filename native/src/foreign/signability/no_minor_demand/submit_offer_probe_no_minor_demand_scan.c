#include "submit_offer_probe_no_minor_demand_internal.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"

static volatile LONG g_kbo_no_minor_background_scan_date = 0;
static volatile LONG g_kbo_no_minor_background_scan_player_count = 0;
static volatile LONG g_kbo_no_minor_background_scan_changed = 1;
static volatile LONG g_kbo_no_minor_background_scan_verify_after_change = 0;

static int kbo_no_minor_is_background_prescan(const char* source)
{
    return source != NULL && strcmp(source, "background_prescan") == 0;
}

int kbo_no_minor_scan_and_floor_teamless_fa_demands(const char* source)
{
    KBO_PROFILE_BEGIN(profile_no_minor_scan);
    if (!kbo_runtime_pause_for_save_if_needed(source != NULL ? source : "no_minor_demand_floor_scan")) {
        KBO_PROFILE_END(profile_no_minor_scan, "no_minor.scan.save_stop");
        return 0;
    }
    if (InterlockedCompareExchange(&g_kbo_no_minor_contract_demand_floor_enabled, 0, 0) == 0) {
        KBO_PROFILE_END(profile_no_minor_scan, "no_minor.scan.disabled");
        return 0;
    }

    uint32_t today = 0u;
    KBO_PROFILE_BEGIN(profile_no_minor_scan_date);
    if (!kbo_current_date_tick_latest_published_date(&today) || today == 0u) {
        KBO_PROFILE_END(profile_no_minor_scan_date, "no_minor.scan.current_date");
        static LONG no_date_log_count = 0;
        LONG slot = InterlockedIncrement(&no_date_log_count);
        if (slot <= 5) {
            kbo_log_runtimef("KBO no-minor demand floor scan skipped source=%s reason=ssot_date_unavailable", source);
        }
        KBO_PROFILE_END(profile_no_minor_scan, "no_minor.scan.no_date");
        return 0;
    }
    KBO_PROFILE_END(profile_no_minor_scan_date, "no_minor.scan.current_date");

    KBO_PROFILE_BEGIN(profile_no_minor_scan_baseline);
    kbo_log_financials_salary_baseline_probe(source);
    KBO_PROFILE_END(profile_no_minor_scan_baseline, "no_minor.scan.log_baseline");

    KBO_PROFILE_BEGIN(profile_no_minor_scan_floor);
    int32_t salary_floor = kbo_no_minor_resolve_current_league_minimum_salary();
    KBO_PROFILE_END(profile_no_minor_scan_floor, "no_minor.scan.resolve_floor");
    if (salary_floor <= 0) {
        static LONG no_floor_log_count = 0;
        LONG slot = InterlockedIncrement(&no_floor_log_count);
        if (slot <= 20) {
            kbo_log_runtimef("KBO no-minor demand floor scan skipped source=%s reason=no_floor", source);
        }
        KBO_PROFILE_END(profile_no_minor_scan, "no_minor.scan.no_floor");
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_no_minor_scan_league);
    uint32_t league_id = (uint32_t)kbo_no_minor_resolve_current_league_id();
    KBO_PROFILE_END(profile_no_minor_scan_league, "no_minor.scan.resolve_league");
    if (league_id == 0u) {
        static LONG no_league_log_count = 0;
        LONG slot = InterlockedIncrement(&no_league_log_count);
        if (slot <= 20) {
            kbo_log_runtimef("KBO no-minor demand floor scan skipped source=%s reason=no_league", source);
        }
        KBO_PROFILE_END(profile_no_minor_scan, "no_minor.scan.no_league");
        return 0;
    }
    KBO_PROFILE_BEGIN(profile_no_minor_scan_financials);
    uint8_t* financials = kbo_resolve_current_league_financials(NULL);
    KBO_PROFILE_END(profile_no_minor_scan_financials, "no_minor.scan.resolve_financials");

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0;
    KBO_PROFILE_BEGIN(profile_no_minor_scan_find_vector);
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        KBO_PROFILE_END(profile_no_minor_scan_find_vector, "no_minor.scan.find_player_vector");
        static LONG no_vector_log_count = 0;
        LONG slot = InterlockedIncrement(&no_vector_log_count);
        if (slot <= 20) {
            kbo_log_runtimef("KBO no-minor demand floor scan skipped source=%s reason=no_player_vector", source);
        }
        KBO_PROFILE_END(profile_no_minor_scan, "no_minor.scan.no_player_vector");
        return 0;
    }
    KBO_PROFILE_END(profile_no_minor_scan_find_vector, "no_minor.scan.find_player_vector");

    int background_prescan = kbo_no_minor_is_background_prescan(source);
    if (background_prescan
            && (uint32_t)InterlockedCompareExchange(&g_kbo_no_minor_background_scan_date, 0, 0) == today
            && InterlockedCompareExchange(&g_kbo_no_minor_background_scan_player_count, 0, 0) == player_count
            && InterlockedCompareExchange(&g_kbo_no_minor_background_scan_changed, 0, 0) == 0
            && InterlockedCompareExchange(&g_kbo_no_minor_background_scan_verify_after_change, 0, 0) == 0) {
        KBO_PROFILE_END(profile_no_minor_scan, "no_minor.scan.cached_unchanged");
        return 0;
    }

    const char* snapshot_failure_reason = NULL;
    KBO_PROFILE_BEGIN(profile_no_minor_scan_snapshot);
    uintptr_t* player_snapshot = kbo_no_minor_copy_player_vector_snapshot(
        player_vector,
        player_count,
        &snapshot_failure_reason);
    KBO_PROFILE_END(profile_no_minor_scan_snapshot, "no_minor.scan.copy_player_vector");
    if (player_snapshot == NULL) {
        static LONG snapshot_fail_log_count = 0;
        LONG slot = InterlockedIncrement(&snapshot_fail_log_count);
        if (slot <= 20 || (slot % 100) == 0) {
            kbo_log_runtimef(
                "KBO no-minor demand floor scan skipped source=%s reason=player_vector_snapshot_failed detail=%s vector=%p count=%d vector_off=0x%x log_slot=%ld",
                source,
                snapshot_failure_reason != NULL ? snapshot_failure_reason : "unknown",
                (void*)player_vector,
                player_count,
                vector_offset,
                slot);
        }
        KBO_PROFILE_END(profile_no_minor_scan, "no_minor.scan.snapshot_failed");
        return 0;
    }

    int scanned = 0;
    int teamless = 0;
    int changed = 0;
    int demand_fixed = 0;
    int retained_demand_fixed = 0;
    int foreign_demand_mapped = 0;
    int level_observed_nonmajor = 0;
    static LONG detail_log_count = 0;
    static LONG foreign_detail_log_count = 0;

    KBO_PROFILE_BEGIN(profile_no_minor_scan_loop);
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = player_snapshot[i];
        uint8_t player_prefilter[KBO_NO_MINOR_SCAN_PREFILTER_BYTES] = {0};
        if (!kbo_no_minor_copy_player_scan_prefix(
                player_ptr,
                player_prefilter,
                sizeof(player_prefilter))) {
            continue;
        }
        scanned++;

        int no_minor_candidate = kbo_no_minor_scan_is_teamless_demand_floor_candidate(player_prefilter, league_id);
        int foreign_candidate = kbo_no_minor_scan_is_foreign_demand_remap_candidate(player_prefilter);
        if (!no_minor_candidate && !foreign_candidate) {
            continue;
        }

        uint8_t player_scan[OOTP27_PLAYER_SCAN_BYTES] = {0};
        if (!kbo_no_minor_copy_player_scan(player_ptr, player_scan)) {
            continue;
        }
        no_minor_candidate = kbo_no_minor_scan_is_teamless_demand_floor_candidate(player_scan, league_id);
        foreign_candidate = kbo_no_minor_scan_is_foreign_demand_remap_candidate(player_scan);
        if (!no_minor_candidate && !foreign_candidate) {
            continue;
        }
        if (no_minor_candidate) {
            teamless++;
        }

        uint32_t player_id = *(uint32_t*)(player_scan + OOTP27_PLAYER_ID_OFFSET);
        int32_t old_demand = *(int32_t*)(player_scan + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
        uint8_t old_contract_level = player_scan[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
        int player_changed = 0;
        uint32_t retained_holder_team_id = 0u;
        int32_t retained_score = 0;
        int retained_index = 0;
        int retained_asian_quota = 0;
        int32_t retained_demand_floor = kbo_foreign_reserve_demand_floor_for_player(
            player_scan,
            today,
            &retained_holder_team_id,
            &retained_score,
            &retained_index,
            &retained_asian_quota);
        if (retained_demand_floor > old_demand
                && kbo_apply_foreign_reserve_demand_floor(player_ptr, source)) {
            retained_demand_fixed++;
            player_changed = 1;
            kbo_no_minor_read_player_i32(player_ptr, OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, &old_demand);
        }
        if (no_minor_candidate && old_contract_level != 1u) {
            level_observed_nonmajor++;
        }
        if (no_minor_candidate && old_demand < salary_floor) {
            if (kbo_no_minor_write_player_i32(player_ptr, OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, salary_floor)) {
                demand_fixed++;
                player_changed = 1;
                old_demand = salary_floor;
            }
        }
        if (foreign_candidate && financials != NULL && retained_demand_floor <= 0) {
            int32_t current_demand = old_demand;
            kbo_no_minor_read_player_i32(player_ptr, OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, &current_demand);
            if (kbo_foreign_fa_demand_remap_already_applied(player_id, current_demand)) {
                continue;
            }
            int asian_quota = kbo_nation_is_asian_quota_candidate(
                *(uint32_t*)(player_scan + OOTP27_PLAYER_NATION_ID_OFFSET));
            int32_t mapped_demand = kbo_foreign_fa_remap_demand_from_salary_ladder(current_demand, financials, asian_quota);
            if (mapped_demand > 0 && mapped_demand < current_demand) {
                if (kbo_no_minor_write_player_i32(player_ptr, OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET, mapped_demand)) {
                    kbo_foreign_fa_demand_remap_remember(player_id, current_demand, mapped_demand);
                    foreign_demand_mapped++;
                    player_changed = 1;
                    LONG foreign_detail_slot = InterlockedIncrement(&foreign_detail_log_count);
                    if (foreign_detail_slot <= kbo_foreign_player_policy()->no_minor_scan_max_detail_logs) {
                        kbo_log_runtimef(
                            "KBO foreign FA demand remapped: source=%s player=%u asian_quota=%d old_demand=%d mapped_demand=%d original_superstar=%d foreign_superstar=%d",
                            source,
                            player_id,
                            asian_quota,
                            current_demand,
                            mapped_demand,
                            memory_range_readable(financials + OOTP27_FINANCIALS_AVERAGE_SALARY_OFFSET, sizeof(int32_t))
                                ? *(int32_t*)(financials + OOTP27_FINANCIALS_AVERAGE_SALARY_OFFSET)
                                : 0,
                            kbo_get_foreign_fa_demand_baseline_value_for_player(8, asian_quota));
                    }
                }
            }
        }
        if (player_changed) {
            changed++;
            LONG detail_slot = InterlockedIncrement(&detail_log_count);
            if (detail_slot <= kbo_foreign_player_policy()->no_minor_scan_max_detail_logs) {
                kbo_log_runtimef(
                    "KBO no-minor demand floor prescan applied: source=%s player=%u old_demand=%d floor=%d observed_contract_level=%u",
                    source,
                    player_id,
                    old_demand,
                    salary_floor,
                    (unsigned)old_contract_level);
            }
        }
    }
    KBO_PROFILE_END(profile_no_minor_scan_loop, "no_minor.scan.player_loop");

    HeapFree(GetProcessHeap(), 0, player_snapshot);

    static LONG summary_log_count = 0;
    LONG summary_slot = changed > 0 ? InterlockedIncrement(&summary_log_count) : 0;
    if (changed > 0 && (summary_slot <= 20 || (summary_slot % 40) == 0)) {
        kbo_log_runtimef(
            "KBO no-minor demand floor prescan complete: source=%s league=%u vector_off=0x%x scanned=%d teamless=%d changed=%d demand_fixed=%d retained_demand_fixed=%d foreign_demand_mapped=%d level_observed_nonmajor=%d floor=%d summary_slot=%ld",
            source,
            league_id,
            vector_offset,
            scanned,
            teamless,
            changed,
            demand_fixed,
            retained_demand_fixed,
            foreign_demand_mapped,
            level_observed_nonmajor,
            salary_floor,
            summary_slot);
    }
    if (background_prescan) {
        InterlockedExchange(&g_kbo_no_minor_background_scan_date, (LONG)today);
        InterlockedExchange(&g_kbo_no_minor_background_scan_player_count, (LONG)player_count);
        if (changed > 0) {
            InterlockedExchange(&g_kbo_no_minor_background_scan_changed, 0);
            InterlockedExchange(&g_kbo_no_minor_background_scan_verify_after_change, 1);
        } else {
            InterlockedExchange(&g_kbo_no_minor_background_scan_changed, 0);
            InterlockedExchange(&g_kbo_no_minor_background_scan_verify_after_change, 0);
        }
    }
    KBO_PROFILE_END(profile_no_minor_scan, changed > 0
        ? "no_minor.scan.total_changed"
        : "no_minor.scan.total_unchanged");
    return changed;
}

