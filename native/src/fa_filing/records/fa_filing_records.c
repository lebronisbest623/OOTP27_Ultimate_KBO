#include "../fa_filing_internal.h"
#include "../../build_verify/build_verify.h"
#include "../../core/csv/core_csv.h"
#include "../../core/files/atomic/core_atomic_file.h"

int kbo_fa_filing_negative_cache_contains(uint32_t player_id)
{
    if (player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < KBO_FA_FILING_NEGATIVE_CACHE_MAX; i++) {
        if (g_kbo_fa_filing_negative_cache[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

void kbo_fa_filing_negative_cache_add(uint32_t player_id)
{
    if (player_id == 0u) {
        return;
    }
    LONG slot = InterlockedIncrement(&g_kbo_fa_filing_negative_cache_cursor);
    g_kbo_fa_filing_negative_cache[(uint32_t)slot % KBO_FA_FILING_NEGATIVE_CACHE_MAX] = player_id;
}

void kbo_fa_filing_negative_cache_clear(void)
{
    memset(g_kbo_fa_filing_negative_cache, 0, sizeof(g_kbo_fa_filing_negative_cache));
    InterlockedExchange(&g_kbo_fa_filing_negative_cache_cursor, 0);
}

int kbo_get_fa_filing_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_filing.csv", out, out_size);
}

void kbo_fa_filing_enter_lock(void)
{
    kbo_lock_enter(&g_kbo_fa_filing_lock);
}

void kbo_fa_filing_leave_lock(void)
{
    kbo_lock_leave(&g_kbo_fa_filing_lock);
}

int kbo_load_fa_filing_records_unlocked(
    KboFaFilingRecord* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_filing_csv_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    int count = 0;
    while (count < max_rows && kbo_csv_reader_next_row(reader)) {
        char fields[10][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 10);
        if (field_count < 8 || fields[0][0] < '0' || fields[0][0] > '9') {
            continue;
        }

        KboFaFilingRecord row;
        memset(&row, 0, sizeof(row));
        row.player_id = kbo_fa_filing_parse_u32(fields[0]);
        row.filing_date = kbo_fa_filing_parse_u32(fields[1]);
        row.season = kbo_fa_filing_parse_u32(fields[2]);
        row.original_team_id = kbo_fa_filing_parse_u32(fields[3]);
        row.league_id = kbo_fa_filing_parse_u32(fields[4]);
        row.source_caller_rva = kbo_fa_filing_parse_u32(fields[5]);
        row.notify = (uint8_t)(kbo_fa_filing_parse_u32(fields[6]) & 0xffu);
        row.contract_level = (uint8_t)(kbo_fa_filing_parse_u32(fields[7]) & 0xffu);
        if (field_count >= 9) {
            kbo_fa_filing_copy_text(row.player_name, sizeof(row.player_name), fields[8]);
        }
        if (field_count >= 10) {
            kbo_fa_filing_copy_text(row.source, sizeof(row.source), fields[9]);
        }
        if (row.player_id != 0u && row.filing_date != 0u && row.season != 0u) {
            rows[count++] = row;
        }
    }

    kbo_csv_reader_close(reader);
    return count;
}

int kbo_write_fa_filing_records_unlocked(
    const KboFaFilingRecord* rows,
    int row_count,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (rows == NULL || row_count < 0 || row_count > KBO_FA_FILING_MAX) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_filing_csv_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef("KBO FA filing csv open failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }

    kbo_fa_filing_write_raw(
        file,
        "player_id,filing_date,season,original_team_id,league_id,source_caller_rva,notify,contract_level,name,source\r\n");

    for (int i = 0; i < row_count; i++) {
        const KboFaFilingRecord* row = &rows[i];
        char prefix[192] = {0};
        snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%u,%u,%u,%u,%u,%u,",
            row->player_id,
            row->filing_date,
            row->season,
            row->original_team_id,
            row->league_id,
            row->source_caller_rva,
            (uint32_t)row->notify,
            (uint32_t)row->contract_level);
        kbo_fa_filing_write_raw(file, prefix);
        kbo_fa_filing_write_csv_text(file, row->player_name);
        kbo_fa_filing_write_raw(file, ",");
        kbo_fa_filing_write_csv_text(file, row->source);
        kbo_fa_filing_write_raw(file, "\r\n");
    }

    if (!kbo_atomic_commit(file, tmp_path, path)) {
        kbo_log_runtimef("KBO FA filing csv atomic commit failed path=%s gle=%lu", path, GetLastError());
        return 0;
    }
    return 1;
}

int kbo_load_fa_filing_records(
    KboFaFilingRecord* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size)
{
    kbo_fa_filing_enter_lock();
    int count = kbo_load_fa_filing_records_unlocked(rows, max_rows, out_path, out_path_size);
    kbo_fa_filing_leave_lock();
    return count;
}

int kbo_fa_filing_find_latest_player(
    uint32_t player_id,
    uint32_t* out_original_team_id,
    uint32_t* out_league_id,
    uint32_t* out_season)
{
    if (out_original_team_id != NULL) {
        *out_original_team_id = 0u;
    }
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }
    if (out_season != NULL) {
        *out_season = 0u;
    }
    if (player_id == 0u) {
        return 0;
    }

    KBO_PROFILE_BEGIN(profile_fa_filing_find_latest);

    char current_path[MAX_PATH] = {0};
    if (!kbo_get_fa_filing_csv_path(current_path, sizeof(current_path))) {
        KBO_PROFILE_END(profile_fa_filing_find_latest, "fa_filing.find_latest.no_path");
        return 0;
    }

    if (InterlockedCompareExchange(&g_kbo_fa_filing_cache_dirty, 0, 0) == 0
            && strcmp(g_kbo_fa_filing_cache_path, current_path) == 0
            && kbo_fa_filing_negative_cache_contains(player_id)) {
        KBO_PROFILE_END(profile_fa_filing_find_latest, "fa_filing.find_latest.negative_cache");
        return 0;
    }

    kbo_fa_filing_enter_lock();
    int needs_reload = g_kbo_fa_filing_cache_rows == NULL
        || InterlockedCompareExchange(&g_kbo_fa_filing_cache_dirty, 0, 0) != 0
        || strcmp(g_kbo_fa_filing_cache_path, current_path) != 0;
    if (needs_reload) {
        if (g_kbo_fa_filing_cache_rows == NULL) {
            g_kbo_fa_filing_cache_rows = (KboFaFilingRecord*)HeapAlloc(
                GetProcessHeap(),
                HEAP_ZERO_MEMORY,
                (SIZE_T)KBO_FA_FILING_MAX * sizeof(KboFaFilingRecord));
        }
        if (g_kbo_fa_filing_cache_rows == NULL) {
            kbo_fa_filing_leave_lock();
            KBO_PROFILE_END(profile_fa_filing_find_latest, "fa_filing.find_latest.alloc_failed");
            return 0;
        }
        memset(g_kbo_fa_filing_cache_rows, 0, (SIZE_T)KBO_FA_FILING_MAX * sizeof(KboFaFilingRecord));
        g_kbo_fa_filing_cache_count = kbo_load_fa_filing_records_unlocked(
            g_kbo_fa_filing_cache_rows,
            KBO_FA_FILING_MAX,
            g_kbo_fa_filing_cache_path,
            sizeof(g_kbo_fa_filing_cache_path));
        kbo_fa_filing_negative_cache_clear();
        InterlockedExchange(&g_kbo_fa_filing_cache_dirty, 0);
    }

    KboFaFilingRecord* rows = g_kbo_fa_filing_cache_rows;
    int row_count = g_kbo_fa_filing_cache_count;
    int best = -1;
    uint32_t best_date = 0u;
    for (int i = 0; i < row_count; i++) {
        if (rows[i].player_id != player_id || rows[i].filing_date == 0u) {
            continue;
        }
        if (best < 0 || rows[i].filing_date >= best_date) {
            best = i;
            best_date = rows[i].filing_date;
        }
    }

    if (best >= 0) {
        if (out_original_team_id != NULL) {
            *out_original_team_id = rows[best].original_team_id;
        }
        if (out_league_id != NULL) {
            *out_league_id = rows[best].league_id;
        }
        if (out_season != NULL) {
            *out_season = rows[best].season;
        }
    }

    kbo_fa_filing_leave_lock();
    if (best < 0) {
        kbo_fa_filing_negative_cache_add(player_id);
    }
    KBO_PROFILE_END(profile_fa_filing_find_latest, best >= 0 ? "fa_filing.find_latest.hit" : "fa_filing.find_latest.miss");
    return best >= 0 ? 1 : 0;
}

int kbo_fa_filing_is_official_transition_caller(uintptr_t caller_rva)
{
    return kbo_current_build_caller_rva_matches(caller_rva, OOTP27_FA_FILING_OFFICIAL_TRANSITION_RETURN_RVA);
}

uint32_t kbo_fa_filing_team_league_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
}

