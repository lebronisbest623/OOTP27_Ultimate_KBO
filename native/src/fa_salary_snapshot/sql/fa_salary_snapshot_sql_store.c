#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_salary_snapshot_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../core/logging/core_log.h"
#include "../../core/sql/escape/core_sql_escape.h"
#include "../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboFaSalarySnapshotSqlExistsResult {
    int found;
} KboFaSalarySnapshotSqlExistsResult;

typedef struct KboFaSalarySnapshotSqlGradeLoadContext {
    KboFaSalarySnapshotGrade* rows;
    int capacity;
    int count;
    int overflowed;
} KboFaSalarySnapshotSqlGradeLoadContext;

static int kbo_fa_salary_snapshot_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS fa_salary_opening_day_snapshots ("
        "season INTEGER NOT NULL,"
        "league_id INTEGER NOT NULL,"
        "player_id INTEGER NOT NULL,"
        "snapshot_date INTEGER NOT NULL DEFAULT 0,"
        "opening_day INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "player_name TEXT NOT NULL DEFAULT '',"
        "nation_id INTEGER NOT NULL DEFAULT 0,"
        "current_team_id INTEGER NOT NULL DEFAULT 0,"
        "active_team_id INTEGER NOT NULL DEFAULT 0,"
        "ranking_team_id INTEGER NOT NULL DEFAULT 0,"
        "current_league_id INTEGER NOT NULL DEFAULT 0,"
        "draft_league_id INTEGER NOT NULL DEFAULT 0,"
        "age INTEGER NOT NULL DEFAULT 0,"
        "retired_flag INTEGER NOT NULL DEFAULT 0,"
        "contract_level INTEGER NOT NULL DEFAULT 0,"
        "foreign_flag INTEGER NOT NULL DEFAULT 0,"
        "salary INTEGER NOT NULL DEFAULT 0,"
        "overall_rank INTEGER NOT NULL DEFAULT 0,"
        "overall_ordinal INTEGER NOT NULL DEFAULT 0,"
        "team_rank INTEGER NOT NULL DEFAULT 0,"
        "team_ordinal INTEGER NOT NULL DEFAULT 0,"
        "contract_status INTEGER NOT NULL DEFAULT 0,"
        "contract_start_year INTEGER NOT NULL DEFAULT 0,"
        "contract_y1 INTEGER NOT NULL DEFAULT 0,"
        "contract_y2 INTEGER NOT NULL DEFAULT 0,"
        "contract_y3 INTEGER NOT NULL DEFAULT 0,"
        "contract_y4 INTEGER NOT NULL DEFAULT 0,"
        "contract_y5 INTEGER NOT NULL DEFAULT 0,"
        "contract_y6 INTEGER NOT NULL DEFAULT 0,"
        "contract_y7 INTEGER NOT NULL DEFAULT 0,"
        "contract_y8 INTEGER NOT NULL DEFAULT 0,"
        "contract_y9 INTEGER NOT NULL DEFAULT 0,"
        "contract_y10 INTEGER NOT NULL DEFAULT 0,"
        "player_key TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(season, league_id, player_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_fa_salary_snapshots_player "
        "ON fa_salary_opening_day_snapshots(player_id, season);"
        "CREATE INDEX IF NOT EXISTS idx_fa_salary_snapshots_team "
        "ON fa_salary_opening_day_snapshots(season, ranking_team_id);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('fa_salary_opening_day_snapshots', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "fa_salary_opening_day_snapshots_schema");
}

int kbo_fa_salary_snapshot_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_fa_salary_snapshot_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static int32_t kbo_fa_salary_snapshot_sql_i32(char** vals, int index)
{
    int value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%d", &value);
    }
    return (int32_t)value;
}

static void kbo_fa_salary_snapshot_sql_text(char** vals, int index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (vals != NULL && vals[index] != NULL) {
        snprintf(out, out_size, "%s", vals[index]);
    }
}

static int kbo_fa_salary_snapshot_sql_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)vals;
    (void)names;
    KboFaSalarySnapshotSqlExistsResult* result = (KboFaSalarySnapshotSqlExistsResult*)user_data;
    if (result != NULL) {
        result->found = 1;
    }
    return 0;
}

int kbo_fa_salary_snapshot_sql_exists(uint32_t season)
{
    if (season == 0u || !kbo_fa_salary_snapshot_sql_ensure_schema("fa_salary_snapshot_exists_schema")) {
        return 0;
    }
    char sql[256] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM fa_salary_opening_day_snapshots WHERE season=%u LIMIT 1;",
        season);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        return 0;
    }

    KboFaSalarySnapshotSqlExistsResult result = {0};
    (void)kbo_save_state_query(sql, kbo_fa_salary_snapshot_sql_exists_cb, &result, "fa_salary_snapshot_exists");
    return result.found;
}

static int kbo_fa_salary_snapshot_sql_append(
    char* out,
    size_t out_size,
    size_t* cursor,
    const char* fmt,
    ...)
{
    if (out == NULL || cursor == NULL || fmt == NULL || *cursor >= out_size) {
        return 0;
    }
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(out + *cursor, out_size - *cursor, fmt, args);
    va_end(args);
    if (len < 0 || (size_t)len >= out_size - *cursor) {
        return 0;
    }
    *cursor += (size_t)len;
    return 1;
}

int kbo_fa_salary_snapshot_sql_replace(
    const KboFaSalarySnapshotRow* rows,
    int row_count,
    uint32_t date,
    uint32_t season,
    uint32_t opening_day,
    uint32_t league_id,
    const char* source)
{
    if (season == 0u || row_count < 0 || (row_count > 0 && rows == NULL)
            || !kbo_fa_salary_snapshot_sql_ensure_schema("fa_salary_snapshot_replace_schema")) {
        return 0;
    }

    size_t sql_size = 1024u + ((size_t)row_count * 1800u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        return 0;
    }

    char escaped_source[128] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        HeapFree(GetProcessHeap(), 0, sql);
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_fa_salary_snapshot_sql_append(
        sql,
        sql_size,
        &cursor,
        "BEGIN IMMEDIATE;DELETE FROM fa_salary_opening_day_snapshots WHERE season=%u AND league_id=%u;",
        season,
        league_id);
    for (int i = 0; ok && i < row_count; i++) {
        const KboFaSalarySnapshotRow* row = &rows[i];
        if (row->player_id == 0u) {
            continue;
        }
        char player_name[224] = {0};
        char player_key[160] = {0};
        if (!kbo_sql_escape_literal(player_name, sizeof(player_name), row->player_name)
                || !kbo_sql_escape_literal(player_key, sizeof(player_key), row->player_key)) {
            ok = 0;
            break;
        }
        ok = kbo_fa_salary_snapshot_sql_append(
            sql,
            sql_size,
            &cursor,
            "INSERT OR REPLACE INTO fa_salary_opening_day_snapshots("
            "season, league_id, player_id, snapshot_date, opening_day, source, player_name, "
            "nation_id, current_team_id, active_team_id, ranking_team_id, current_league_id, draft_league_id, "
            "age, retired_flag, contract_level, foreign_flag, salary, overall_rank, overall_ordinal, "
            "team_rank, team_ordinal, contract_status, contract_start_year, "
            "contract_y1, contract_y2, contract_y3, contract_y4, contract_y5, contract_y6, contract_y7, "
            "contract_y8, contract_y9, contract_y10, player_key, updated_at"
            ") VALUES(%u, %u, %u, %u, %u, '%s', '%s', %u, %u, %u, %u, %u, %u, "
            "%u, %u, %u, %u, %d, %u, %u, %u, %u, %d, %d, "
            "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, '%s', datetime('now'));",
            season,
            league_id,
            row->player_id,
            date,
            opening_day,
            escaped_source,
            player_name,
            row->nation_id,
            row->current_team_id,
            row->active_team_id,
            row->ranking_team_id,
            row->current_league_id,
            row->draft_league_id,
            (uint32_t)row->age,
            (uint32_t)row->retired_flag,
            (uint32_t)row->contract_level,
            (uint32_t)row->foreign_flag,
            row->salary,
            row->overall_rank,
            row->overall_ordinal,
            row->team_rank,
            row->team_ordinal,
            row->contract_status,
            row->contract_start_year,
            row->contract_years[0],
            row->contract_years[1],
            row->contract_years[2],
            row->contract_years[3],
            row->contract_years[4],
            row->contract_years[5],
            row->contract_years[6],
            row->contract_years[7],
            row->contract_years[8],
            row->contract_years[9],
            player_key);
    }
    if (ok) {
        ok = kbo_fa_salary_snapshot_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef("KBO FA salary snapshot sqlite replace failed reason=sql_buffer_full rows=%d", row_count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "fa_salary_snapshot_replace");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}

static int kbo_fa_salary_snapshot_sql_grade_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboFaSalarySnapshotSqlGradeLoadContext* ctx = (KboFaSalarySnapshotSqlGradeLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 14) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboFaSalarySnapshotGrade row = {0};
    row.snapshot_date = kbo_fa_salary_snapshot_sql_u32(vals, 0);
    row.season = kbo_fa_salary_snapshot_sql_u32(vals, 1);
    row.opening_day = kbo_fa_salary_snapshot_sql_u32(vals, 2);
    row.player_id = kbo_fa_salary_snapshot_sql_u32(vals, 3);
    kbo_fa_salary_snapshot_sql_text(vals, 4, row.player_name, sizeof(row.player_name));
    row.ranking_team_id = kbo_fa_salary_snapshot_sql_u32(vals, 5);
    row.foreign_flag = kbo_fa_salary_snapshot_sql_u32(vals, 6) != 0u ? 1u : 0u;
    row.salary = kbo_fa_salary_snapshot_sql_i32(vals, 7);
    row.overall_rank = kbo_fa_salary_snapshot_sql_u32(vals, 8);
    row.overall_ordinal = kbo_fa_salary_snapshot_sql_u32(vals, 9);
    row.team_rank = kbo_fa_salary_snapshot_sql_u32(vals, 10);
    row.team_ordinal = kbo_fa_salary_snapshot_sql_u32(vals, 11);
    kbo_fa_salary_snapshot_sql_text(vals, 12, row.player_key, sizeof(row.player_key));
    if (row.player_id == 0u || row.season == 0u) {
        return 0;
    }

    ctx->rows[ctx->count++] = row;
    return 0;
}

int kbo_fa_salary_snapshot_sql_load_grade_rows(
    uint32_t season,
    KboFaSalarySnapshotGrade* rows,
    int max_rows,
    int* out_count)
{
    if (rows == NULL || max_rows <= 0 || out_count == NULL
            || !kbo_fa_salary_snapshot_sql_ensure_schema("fa_salary_snapshot_grade_load_schema")) {
        return 0;
    }
    *out_count = 0;

    char sql[1024] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "SELECT snapshot_date, season, opening_day, player_id, player_name, ranking_team_id, "
        "foreign_flag, salary, overall_rank, overall_ordinal, team_rank, team_ordinal, player_key, league_id "
        "FROM fa_salary_opening_day_snapshots WHERE season=%u ORDER BY overall_ordinal, player_id;",
        season);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        return 0;
    }

    KboFaSalarySnapshotSqlGradeLoadContext ctx = {rows, max_rows, 0, 0};
    if (!kbo_save_state_query(sql, kbo_fa_salary_snapshot_sql_grade_cb, &ctx, "fa_salary_snapshot_grade_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "KBO FA salary snapshot sqlite grade load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max_rows);
    }
    *out_count = ctx.count;
    return 1;
}
