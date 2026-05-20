#include "salary_snapshot_write_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../core/logging/core_log.h"
#include "../../fa_market_classification/policy/fa_market_policy.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"
#include "../audit/salary_snapshot_rule_audit.h"
#include "../csv/salary_snapshot_csv_parse.h"
#include "../paths/salary_snapshot_paths_dates.h"
#include "../sql/fa_salary_snapshot_sql_store.h"
#include "salary_snapshot_memory_key.h"
#include "../ranking/salary_snapshot_row_ranking.h"

static uintptr_t* kbo_fa_salary_snapshot_copy_player_vector_snapshot(
    uintptr_t player_vector,
    int32_t player_count,
    const char** out_failure_reason)
{
    if (out_failure_reason != NULL) {
        *out_failure_reason = "unknown";
    }
    if (player_vector == 0u || player_count <= 0 || player_count > KBO_RUNTIME_MAX_PLAYER_VECTOR_COUNT) {
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

int kbo_fa_salary_snapshot_write_csv(
    const KboFaSalarySnapshotRow* rows,
    int row_count,
    uint32_t date,
    uint32_t season,
    uint32_t opening_day,
    uint32_t league_id,
    const char* source,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }

    char path[MAX_PATH] = {0};
    if (!kbo_fa_salary_snapshot_path(season, path, sizeof(path))) {
        kbo_log_runtimef("KBO FA salary snapshot skipped source=%s reason=path_unavailable season=%u", source != NULL ? source : "", season);
        return 0;
    }

    if (!kbo_fa_salary_snapshot_sql_replace(
            rows,
            row_count,
            date,
            season,
            opening_day,
            league_id,
            source)) {
        kbo_log_runtimef("KBO FA salary snapshot sqlite write failed path=%s", path);
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    return 1;
}

int kbo_capture_fa_salary_opening_day_snapshot(const char* source, uint32_t date, uint32_t season, uint32_t opening_day, uint32_t league_id)
{
    LARGE_INTEGER profile_snapshot_capture = {0};
    int profile_snapshot_capture_active = kbo_profiler_begin(&profile_snapshot_capture);

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        kbo_log_runtimef("KBO FA salary snapshot skipped source=%s reason=no_player_vector date=%u", source != NULL ? source : "", date);
        kbo_audit_fa_salary_snapshot_capture_skip(
            "player_vector_unavailable",
            source,
            date,
            season,
            opening_day,
            league_id,
            0);
        if (profile_snapshot_capture_active) {
            kbo_profiler_end("fa_salary_snapshot.capture.no_player_vector", &profile_snapshot_capture);
        }
        return 0;
    }

    const char* snapshot_failure_reason = NULL;
    uintptr_t* player_snapshot = kbo_fa_salary_snapshot_copy_player_vector_snapshot(
        player_vector,
        player_count,
        &snapshot_failure_reason);
    if (player_snapshot == NULL) {
        kbo_log_runtimef(
            "KBO FA salary snapshot skipped source=%s reason=player_vector_snapshot_failed detail=%s vector=%p count=%d",
            source != NULL ? source : "",
            snapshot_failure_reason != NULL ? snapshot_failure_reason : "",
            (void*)player_vector,
            player_count);
        kbo_audit_fa_salary_snapshot_capture_skip(
            snapshot_failure_reason != NULL ? snapshot_failure_reason : "player_vector_snapshot_failed",
            source,
            date,
            season,
            opening_day,
            league_id,
            player_count);
        if (profile_snapshot_capture_active) {
            kbo_profiler_end("fa_salary_snapshot.capture.snapshot_failed", &profile_snapshot_capture);
        }
        return 0;
    }

    KboFaSalarySnapshotRow* rows = (KboFaSalarySnapshotRow*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)player_count * sizeof(KboFaSalarySnapshotRow));
    if (rows == NULL) {
        kbo_log_runtimef("KBO FA salary snapshot skipped source=%s reason=alloc_failed count=%d", source != NULL ? source : "", player_count);
        kbo_audit_fa_salary_snapshot_capture_skip(
            "allocation_failed",
            source,
            date,
            season,
            opening_day,
            league_id,
            player_count);
        HeapFree(GetProcessHeap(), 0, player_snapshot);
        if (profile_snapshot_capture_active) {
            kbo_profiler_end("fa_salary_snapshot.capture.alloc_failed", &profile_snapshot_capture);
        }
        return 0;
    }

    int row_count = 0;
    int scanned = 0;
    int salary_rows = 0;
    int zero_salary_rows = 0;
    const KboFaMarketPolicy* policy = kbo_fa_market_policy();
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = player_snapshot[i];
        if (!kbo_player_pointer_plausible(player_ptr)
                || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }

        scanned++;
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t ranking_team_id = kbo_fa_salary_snapshot_resolve_ranking_team(league_id, current_team_id, active_team_id);
        if (ranking_team_id == 0u) {
            continue;
        }

        uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        uint8_t retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
        if (retired_flag != 0u
                || age < (uint16_t)policy->player_age_min
                || age > (uint16_t)policy->player_age_max) {
            continue;
        }

        KboFaSalarySnapshotRow* row = &rows[row_count++];
        row->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        row->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        row->current_team_id = current_team_id;
        row->active_team_id = active_team_id;
        row->ranking_team_id = ranking_team_id;
        row->current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        row->draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
        row->age = age;
        row->retired_flag = retired_flag;
        row->contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
        row->foreign_flag = row->nation_id != OOTP27_KBO_KOREA_NATION_ID ? 1u : 0u;
        row->salary = kbo_fa_salary_snapshot_player_salary_for_season(player, season);
        kbo_fa_salary_snapshot_copy_contract_years(player, row);
        kbo_copy_player_display_name(player, row->player_name, sizeof(row->player_name));
        if (row->player_name[0] == '\0' || strcmp(row->player_name, "Unknown player") == 0) {
            snprintf(row->player_name, sizeof(row->player_name), "Player #%u", row->player_id);
        }
        kbo_fa_salary_snapshot_copy_memory_player_key(player, row->player_key, sizeof(row->player_key));

        if (row->salary > 0) {
            salary_rows++;
        } else {
            zero_salary_rows++;
        }
    }

    kbo_fa_salary_snapshot_assign_overall_ranks(rows, row_count);
    kbo_fa_salary_snapshot_assign_team_ranks(rows, row_count);
    qsort(rows, (size_t)row_count, sizeof(KboFaSalarySnapshotRow), kbo_fa_salary_snapshot_compare_overall);

    char path[MAX_PATH] = {0};
    int wrote = kbo_fa_salary_snapshot_write_csv(rows, row_count, date, season, opening_day, league_id, source, path, sizeof(path));
    if (wrote) {
        InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_season, (LONG)season);
        InterlockedExchange(&g_kbo_fa_salary_snapshot_cached_exists_value, 1);
        kbo_log_runtimef(
            "KBO FA salary opening-day snapshot written source=%s date=%u season=%u opening_day=%u league=%u scanned=%d rows=%d salary_rows=%d zero_salary_rows=%d csv=%s",
            source != NULL ? source : "",
            date,
            season,
            opening_day,
            league_id,
            scanned,
            row_count,
            salary_rows,
            zero_salary_rows,
            path);
        kbo_audit_fa_salary_snapshot_capture_result(
            "write_snapshot",
            "opening_day_snapshot_written",
            source,
            date,
            season,
            opening_day,
            league_id,
            scanned,
            row_count,
            salary_rows,
            zero_salary_rows);
    } else {
        kbo_audit_fa_salary_snapshot_capture_result(
            "fail",
            "csv_write_failed",
            source,
            date,
            season,
            opening_day,
            league_id,
            scanned,
            row_count,
            salary_rows,
            zero_salary_rows);
    }

    HeapFree(GetProcessHeap(), 0, rows);
    HeapFree(GetProcessHeap(), 0, player_snapshot);
    if (profile_snapshot_capture_active) {
        kbo_profiler_end("fa_salary_snapshot.capture", &profile_snapshot_capture);
    }
    return wrote;
}
