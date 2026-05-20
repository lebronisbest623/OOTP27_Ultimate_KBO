#include "../internal/amateur_assignment_internal.h"
#include "../../core/csv/core_csv.h"
#include "../../core/sql/save_state/save_state_sqlite.h"
#include "../../core/sync/spin_lock.h"
#include "sql/amateur_reputation_sql_store.h"

#define KBO_AMATEUR_REPUTATION_HISTORY_LOAD_MAX 32768

void kbo_lock_amateur_reputation_seeds(void)
{
    kbo_spin_lock(&g_kbo_amateur_reputation_seed_lock);
}

void kbo_unlock_amateur_reputation_seeds(void)
{
    kbo_spin_unlock(&g_kbo_amateur_reputation_seed_lock);
}

void kbo_lock_amateur_assignment_candidates(void)
{
    kbo_spin_lock(&g_kbo_amateur_assignment_candidate_lock);
}

void kbo_unlock_amateur_assignment_candidates(void)
{
    kbo_spin_unlock(&g_kbo_amateur_assignment_candidate_lock);
}

void kbo_invalidate_amateur_assignment_candidate_cache(void)
{
    kbo_lock_amateur_assignment_candidates();
    g_kbo_amateur_assignment_high_school_count = -1;
    g_kbo_amateur_assignment_college_count = -1;
    g_kbo_amateur_resolved_team_reputation_count = 0;
    InterlockedExchange(&g_kbo_amateur_assignment_rejected_target_count, 0);
    memset(g_kbo_amateur_assignment_high_school_candidates, 0, sizeof(g_kbo_amateur_assignment_high_school_candidates));
    memset(g_kbo_amateur_assignment_college_candidates, 0, sizeof(g_kbo_amateur_assignment_college_candidates));
    memset(g_kbo_amateur_resolved_team_reputations, 0, sizeof(g_kbo_amateur_resolved_team_reputations));
    memset(g_kbo_amateur_assignment_rejected_targets, 0, sizeof(g_kbo_amateur_assignment_rejected_targets));
    kbo_unlock_amateur_assignment_candidates();
}

int kbo_get_reputation_seed_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || file_name[0] == '\0' || out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';

    return kbo_get_global_data_file(file_name, out, out_size);
}

int kbo_amateur_reputation_add_seed(
    uint32_t league_id,
    uint32_t team_id,
    const char* team_abbr,
    const char* team_name,
    const char* nick_name,
    uint32_t reputation)
{
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }
    if (reputation < 1u) {
        reputation = 1u;
    } else if (reputation > 100u) {
        reputation = 100u;
    }

    for (int i = 0; i < g_kbo_amateur_reputation_seed_count; i++) {
        if (g_kbo_amateur_reputation_seeds[i].league_id == league_id
                && g_kbo_amateur_reputation_seeds[i].team_id == team_id) {
            if (team_abbr != NULL && team_abbr[0] != '\0') {
                snprintf(g_kbo_amateur_reputation_seeds[i].team_abbr, sizeof(g_kbo_amateur_reputation_seeds[i].team_abbr), "%s", team_abbr);
            }
            if (team_name != NULL && team_name[0] != '\0') {
                snprintf(g_kbo_amateur_reputation_seeds[i].team_name, sizeof(g_kbo_amateur_reputation_seeds[i].team_name), "%s", team_name);
            }
            if (nick_name != NULL && nick_name[0] != '\0') {
                snprintf(g_kbo_amateur_reputation_seeds[i].nick_name, sizeof(g_kbo_amateur_reputation_seeds[i].nick_name), "%s", nick_name);
            }
            g_kbo_amateur_reputation_seeds[i].reputation = (uint8_t)reputation;
            return 1;
        }
    }
    if (g_kbo_amateur_reputation_seed_count >= KBO_AMATEUR_REPUTATION_SEED_MAX) {
        return 0;
    }
    g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].league_id = league_id;
    g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_id = team_id;
    snprintf(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_abbr, sizeof(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_abbr), "%s", team_abbr != NULL ? team_abbr : "");
    snprintf(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_name, sizeof(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].team_name), "%s", team_name != NULL ? team_name : "");
    snprintf(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].nick_name, sizeof(g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].nick_name), "%s", nick_name != NULL ? nick_name : "");
    g_kbo_amateur_reputation_seeds[g_kbo_amateur_reputation_seed_count].reputation = (uint8_t)reputation;
    g_kbo_amateur_reputation_seed_count++;
    return 1;
}

int kbo_load_reputation_seed_path_into_cache(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        kbo_log_runtimef("team reputation seed load skipped path=%s reason=read_failed", path);
        return 0;
    }

    int before = g_kbo_amateur_reputation_seed_count;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[10][128];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 10);
        if (field_count <= 0
                || fields[0][0] == '\0'
                || fields[0][0] == '#'
                || _stricmp(fields[0], "league_id") == 0) {
            continue;
        }

        uint32_t league_id = kbo_csv_parse_u32_text(fields[0], 10);
        uint32_t team_id = field_count > 5 ? kbo_csv_parse_u32_text(fields[5], 10) : 0u;
        const char* team_abbr = field_count > 6 ? fields[6] : "";
        const char* team_name = field_count > 7 ? fields[7] : "";
        const char* nick_name = field_count > 8 ? fields[8] : "";
        uint32_t reputation = field_count > 9 ? kbo_csv_parse_u32_text(fields[9], 10) : 0u;
        kbo_amateur_reputation_add_seed(league_id, team_id, team_abbr, team_name, nick_name, reputation);
    }

    int added = g_kbo_amateur_reputation_seed_count - before;
    kbo_csv_reader_close(reader);
    kbo_log_runtimef("team reputation seed loaded added=%d total=%d path=%s", added, g_kbo_amateur_reputation_seed_count, path);
    return 1;
}

int kbo_get_amateur_reputation_history_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    out[0] = '\0';
    return kbo_save_state_db_path(out, out_size);
}

int kbo_amateur_reputation_history_has_year(uint32_t league_id, uint32_t year)
{
    return kbo_amateur_reputation_sql_history_has_year(league_id, year);
}

static int kbo_amateur_reputation_cached_value(uint32_t league_id, uint32_t team_id, uint8_t* out_reputation)
{
    if (out_reputation != NULL) {
        *out_reputation = 0u;
    }
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }

    for (int i = 0; i < g_kbo_amateur_reputation_seed_count; i++) {
        if (g_kbo_amateur_reputation_seeds[i].league_id == league_id
                && g_kbo_amateur_reputation_seeds[i].team_id == team_id) {
            if (out_reputation != NULL) {
                *out_reputation = g_kbo_amateur_reputation_seeds[i].reputation;
            }
            return 1;
        }
    }

    return 0;
}

static int kbo_apply_amateur_reputation_history_rows_to_cache(
    const KboAmateurReputationHistoryRow* rows,
    int row_count,
    const char* path)
{
    if (rows == NULL || row_count <= 0) {
        return 0;
    }

    int applied = 0;
    int repaired = 0;
    for (int i = 0; i < row_count; i++) {
        const KboAmateurReputationHistoryRow* row = &rows[i];
        uint32_t league_id = row->league_id;
        uint32_t team_id = row->team_id;
        uint32_t old_reputation = row->old_reputation;
        int32_t delta = row->delta;
        uint32_t recorded_new_reputation = row->new_reputation;
        uint32_t new_reputation = recorded_new_reputation;
        uint8_t current_reputation = 0u;
        if (old_reputation != 0u
                && kbo_amateur_reputation_cached_value(league_id, team_id, &current_reputation)
                && (uint32_t)current_reputation != old_reputation) {
            new_reputation = (uint32_t)kbo_amateur_reputation_clamp_for_league(
                league_id,
                (int32_t)current_reputation + delta);
            repaired++;
        } else {
            new_reputation = (uint32_t)kbo_amateur_reputation_clamp_for_league(
                league_id,
                (int32_t)recorded_new_reputation);
        }
        if (league_id != 0u && team_id != 0u && new_reputation != 0u
                && kbo_amateur_reputation_add_seed(league_id, team_id, "", "", "", new_reputation)) {
            applied++;
        }
    }

    kbo_log_runtimef(
        "amateur reputation history replayed rows=%d repaired=%d path=%s store=sqlite",
        applied,
        repaired,
        path != NULL ? path : "");
    return applied;
}

int kbo_apply_amateur_reputation_history_to_cache(void)
{
    char path[MAX_PATH] = {0};
    kbo_get_amateur_reputation_history_path(path, sizeof(path));

    KboAmateurReputationHistoryRow* rows = (KboAmateurReputationHistoryRow*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(KboAmateurReputationHistoryRow) * KBO_AMATEUR_REPUTATION_HISTORY_LOAD_MAX);
    if (rows == NULL) {
        return 0;
    }

    int row_count = 0;
    int ok = kbo_amateur_reputation_sql_history_load(
        rows,
        KBO_AMATEUR_REPUTATION_HISTORY_LOAD_MAX,
        &row_count);
    int applied = ok ? kbo_apply_amateur_reputation_history_rows_to_cache(rows, row_count, path) : 0;
    HeapFree(GetProcessHeap(), 0, rows);
    return applied;
}

void kbo_ensure_amateur_reputation_seeds_loaded(void)
{
    char high_school_path[MAX_PATH] = {0};
    char college_path[MAX_PATH] = {0};
    char history_path[MAX_PATH] = {0};
    kbo_get_reputation_seed_path("high_school_reputation_seed.csv", high_school_path, sizeof(high_school_path));
    kbo_get_reputation_seed_path("college_reputation_seed.csv", college_path, sizeof(college_path));
    kbo_get_amateur_reputation_history_path(history_path, sizeof(history_path));

    char loaded_key[MAX_PATH * 3] = {0};
    snprintf(loaded_key, sizeof(loaded_key), "%s|%s|%s", high_school_path, college_path, history_path);
    if (InterlockedCompareExchange(&g_kbo_amateur_reputation_seed_loaded, 1, 1) == 1
            && strcmp(loaded_key, g_kbo_amateur_reputation_seed_loaded_path) == 0) {
        return;
    }

    KboAmateurReputationHistoryRow* history_rows = (KboAmateurReputationHistoryRow*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(KboAmateurReputationHistoryRow) * KBO_AMATEUR_REPUTATION_HISTORY_LOAD_MAX);
    int history_count = 0;
    if (history_rows != NULL) {
        kbo_amateur_reputation_sql_history_load(
            history_rows,
            KBO_AMATEUR_REPUTATION_HISTORY_LOAD_MAX,
            &history_count);
    }

    kbo_lock_amateur_reputation_seeds();
    memset(g_kbo_amateur_reputation_seeds, 0, sizeof(g_kbo_amateur_reputation_seeds));
    g_kbo_amateur_reputation_seed_count = 0;
    if (high_school_path[0] != '\0') {
        kbo_load_reputation_seed_path_into_cache(high_school_path);
    }
    if (college_path[0] != '\0') {
        kbo_load_reputation_seed_path_into_cache(college_path);
    }
    if (history_rows != NULL) {
        kbo_apply_amateur_reputation_history_rows_to_cache(history_rows, history_count, history_path);
    }
    strncpy(g_kbo_amateur_reputation_seed_loaded_path, loaded_key, sizeof(g_kbo_amateur_reputation_seed_loaded_path) - 1u);
    g_kbo_amateur_reputation_seed_loaded_path[sizeof(g_kbo_amateur_reputation_seed_loaded_path) - 1u] = '\0';
    InterlockedExchange(&g_kbo_amateur_reputation_seed_loaded, 1);
    int count = g_kbo_amateur_reputation_seed_count;
    kbo_unlock_amateur_reputation_seeds();
    if (history_rows != NULL) {
        HeapFree(GetProcessHeap(), 0, history_rows);
    }
    kbo_log_runtimef("team reputation seeds ready count=%d", count);
}

int kbo_find_amateur_team_reputation_for_league(uint32_t league_id, uint32_t team_id, uint8_t* out_reputation)
{
    if (out_reputation != NULL) {
        *out_reputation = kbo_amateur_default_team_reputation();
    }
    if (league_id == 0u || team_id == 0u) {
        return 0;
    }
    kbo_ensure_amateur_reputation_seeds_loaded();
    kbo_lock_amateur_reputation_seeds();
    for (int i = 0; i < g_kbo_amateur_reputation_seed_count; i++) {
        if (g_kbo_amateur_reputation_seeds[i].league_id == league_id
                && g_kbo_amateur_reputation_seeds[i].team_id == team_id) {
            if (out_reputation != NULL) {
                *out_reputation = g_kbo_amateur_reputation_seeds[i].reputation;
            }
            kbo_unlock_amateur_reputation_seeds();
            return 1;
        }
    }
    kbo_unlock_amateur_reputation_seeds();
    return 0;
}

