#include "../fa_filing_internal.h"
#include "../../build_verify/build_verify.h"
#include "sql/fa_filing_sql_store.h"

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
    return kbo_fa_filing_sql_path(out, out_size);
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

    if (out_path != NULL && out_path_size > 0) {
        (void)kbo_get_fa_filing_csv_path(out_path, out_path_size);
    }

    int count = 0;
    if (!kbo_fa_filing_sql_load(rows, max_rows, &count)) {
        return 0;
    }
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

    if (out_path != NULL && out_path_size > 0) {
        (void)kbo_get_fa_filing_csv_path(out_path, out_path_size);
    }
    return kbo_fa_filing_sql_replace_all(rows, row_count);
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

