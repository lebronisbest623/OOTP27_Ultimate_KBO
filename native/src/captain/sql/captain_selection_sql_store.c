#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "captain_selection_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../core/logging/core_log.h"
#include "../../core/sql/escape/core_sql_escape.h"
#include "../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboCaptainSelectionSqlExistsResult {
    int found;
} KboCaptainSelectionSqlExistsResult;

typedef struct KboCaptainSelectionSqlLoadContext {
    KboCaptainSelectionRow* rows;
    int capacity;
    int count;
    int overflowed;
} KboCaptainSelectionSqlLoadContext;

static int kbo_captain_selection_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS captain_selections ("
        "season INTEGER NOT NULL,"
        "team_id INTEGER NOT NULL,"
        "date INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "league_id INTEGER NOT NULL DEFAULT 0,"
        "team_name TEXT NOT NULL DEFAULT '',"
        "captain_player_id INTEGER NOT NULL DEFAULT 0,"
        "captain_name TEXT NOT NULL DEFAULT '',"
        "score INTEGER NOT NULL DEFAULT 0,"
        "reason TEXT NOT NULL DEFAULT '',"
        "seeded INTEGER NOT NULL DEFAULT 0,"
        "seed_priority INTEGER NOT NULL DEFAULT 0,"
        "seed_source TEXT NOT NULL DEFAULT '',"
        "nation_id INTEGER NOT NULL DEFAULT 0,"
        "domestic INTEGER NOT NULL DEFAULT 0,"
        "current_team_id INTEGER NOT NULL DEFAULT 0,"
        "active_team_id INTEGER NOT NULL DEFAULT 0,"
        "current_league_id INTEGER NOT NULL DEFAULT 0,"
        "age INTEGER NOT NULL DEFAULT 0,"
        "salary INTEGER NOT NULL DEFAULT 0,"
        "value_score INTEGER NOT NULL DEFAULT 0,"
        "same_team_seasons INTEGER NOT NULL DEFAULT 0,"
        "overall_value INTEGER NOT NULL DEFAULT 0,"
        "talent_value INTEGER NOT NULL DEFAULT 0,"
        "ratings_value INTEGER NOT NULL DEFAULT 0,"
        "career_value INTEGER NOT NULL DEFAULT 0,"
        "dfa INTEGER NOT NULL DEFAULT 0,"
        "restricted INTEGER NOT NULL DEFAULT 0,"
        "injured INTEGER NOT NULL DEFAULT 0,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(season, team_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_captain_selections_player "
        "ON captain_selections(captain_player_id, season);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('captain_selections', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "captain_selections_schema");
}

int kbo_captain_selection_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_captain_selection_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static int32_t kbo_captain_selection_sql_i32(char** vals, int index)
{
    int value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%d", &value);
    }
    return (int32_t)value;
}

static void kbo_captain_selection_sql_text(char** vals, int index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (vals != NULL && vals[index] != NULL) {
        snprintf(out, out_size, "%s", vals[index]);
    }
}

static int kbo_captain_selection_sql_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)vals;
    (void)names;
    KboCaptainSelectionSqlExistsResult* result = (KboCaptainSelectionSqlExistsResult*)user_data;
    if (result != NULL) {
        result->found = 1;
    }
    return 0;
}

int kbo_captain_selection_sql_exists(uint32_t season)
{
    if (season == 0u || !kbo_captain_selection_sql_ensure_schema("captain_selections_exists_schema")) {
        return 0;
    }
    char sql[192] = {0};
    int len = snprintf(sql, sizeof(sql), "SELECT 1 FROM captain_selections WHERE season=%u LIMIT 1;", season);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        return 0;
    }

    KboCaptainSelectionSqlExistsResult result = {0};
    (void)kbo_save_state_query(sql, kbo_captain_selection_sql_exists_cb, &result, "captain_selections_exists");
    return result.found;
}

static int kbo_captain_selection_sql_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboCaptainSelectionSqlLoadContext* ctx = (KboCaptainSelectionSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 28) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboCaptainSelectionRow row = {0};
    row.date = kbo_captain_selection_sql_u32(vals, 0);
    row.season = kbo_captain_selection_sql_u32(vals, 1);
    row.league_id = kbo_captain_selection_sql_u32(vals, 2);
    row.team_id = kbo_captain_selection_sql_u32(vals, 3);
    kbo_captain_selection_sql_text(vals, 4, row.team_name, sizeof(row.team_name));
    row.player_id = kbo_captain_selection_sql_u32(vals, 5);
    kbo_captain_selection_sql_text(vals, 6, row.player_name, sizeof(row.player_name));
    row.score = kbo_captain_selection_sql_i32(vals, 7);
    kbo_captain_selection_sql_text(vals, 8, row.reason, sizeof(row.reason));
    row.seeded = kbo_captain_selection_sql_u32(vals, 9) != 0u ? 1u : 0u;
    row.seed_priority = kbo_captain_selection_sql_i32(vals, 10);
    kbo_captain_selection_sql_text(vals, 11, row.seed_source, sizeof(row.seed_source));
    row.nation_id = kbo_captain_selection_sql_u32(vals, 12);
    row.domestic = kbo_captain_selection_sql_u32(vals, 13) != 0u ? 1u : 0u;
    row.current_team_id = kbo_captain_selection_sql_u32(vals, 14);
    row.active_team_id = kbo_captain_selection_sql_u32(vals, 15);
    row.current_league_id = kbo_captain_selection_sql_u32(vals, 16);
    row.age = (uint16_t)(kbo_captain_selection_sql_u32(vals, 17) & 0xffffu);
    row.salary = kbo_captain_selection_sql_i32(vals, 18);
    row.value_score = kbo_captain_selection_sql_i32(vals, 19);
    row.same_team_seasons = kbo_captain_selection_sql_i32(vals, 20);
    row.overall_value = (int16_t)kbo_captain_selection_sql_i32(vals, 21);
    row.talent_value = (int16_t)kbo_captain_selection_sql_i32(vals, 22);
    row.ratings_value = (int16_t)kbo_captain_selection_sql_i32(vals, 23);
    row.career_value = (int16_t)kbo_captain_selection_sql_i32(vals, 24);
    row.dfa = kbo_captain_selection_sql_u32(vals, 25) != 0u ? 1u : 0u;
    row.restricted = kbo_captain_selection_sql_u32(vals, 26) != 0u ? 1u : 0u;
    row.injured = kbo_captain_selection_sql_u32(vals, 27) != 0u ? 1u : 0u;
    if (row.season == 0u || row.team_id == 0u) {
        return 0;
    }

    ctx->rows[ctx->count++] = row;
    return 0;
}

int kbo_captain_selection_sql_load(uint32_t season, KboCaptainSelectionRow* rows, int max_rows, int* out_count)
{
    if (rows == NULL || max_rows <= 0 || out_count == NULL
            || !kbo_captain_selection_sql_ensure_schema("captain_selections_load_schema")) {
        return 0;
    }
    *out_count = 0;
    char sql[1024] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "SELECT date, season, league_id, team_id, team_name, captain_player_id, captain_name, score, "
        "reason, seeded, seed_priority, seed_source, nation_id, domestic, current_team_id, active_team_id, "
        "current_league_id, age, salary, value_score, same_team_seasons, overall_value, talent_value, "
        "ratings_value, career_value, dfa, restricted, injured "
        "FROM captain_selections WHERE season=%u ORDER BY league_id, team_id;",
        season);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        return 0;
    }

    KboCaptainSelectionSqlLoadContext ctx = {rows, max_rows, 0, 0};
    if (!kbo_save_state_query(sql, kbo_captain_selection_sql_load_cb, &ctx, "captain_selections_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef("KBO captain selection sqlite load truncated rows=%d capacity=%d", ctx.overflowed, max_rows);
    }
    *out_count = ctx.count;
    return 1;
}

static int kbo_captain_selection_sql_append(
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

int kbo_captain_selection_sql_replace_season(
    const KboCaptainSelectionRow* rows,
    int row_count,
    const char* source)
{
    if (rows == NULL || row_count <= 0
            || !kbo_captain_selection_sql_ensure_schema("captain_selections_replace_schema")) {
        return 0;
    }

    uint32_t season = rows[0].season;
    if (season == 0u) {
        return 0;
    }

    size_t sql_size = 1024u + ((size_t)row_count * 1400u);
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
    int ok = kbo_captain_selection_sql_append(
        sql,
        sql_size,
        &cursor,
        "BEGIN IMMEDIATE;DELETE FROM captain_selections WHERE season=%u;",
        season);
    for (int i = 0; ok && i < row_count; i++) {
        const KboCaptainSelectionRow* row = &rows[i];
        if (row->season == 0u || row->team_id == 0u) {
            continue;
        }
        char team_name[280] = {0};
        char player_name[280] = {0};
        char reason[224] = {0};
        char seed_source[80] = {0};
        if (!kbo_sql_escape_literal(team_name, sizeof(team_name), row->team_name)
                || !kbo_sql_escape_literal(player_name, sizeof(player_name), row->player_name)
                || !kbo_sql_escape_literal(reason, sizeof(reason), row->reason)
                || !kbo_sql_escape_literal(seed_source, sizeof(seed_source), row->seed_source)) {
            ok = 0;
            break;
        }
        ok = kbo_captain_selection_sql_append(
            sql,
            sql_size,
            &cursor,
            "INSERT OR REPLACE INTO captain_selections("
            "season, team_id, date, source, league_id, team_name, captain_player_id, captain_name, score, "
            "reason, seeded, seed_priority, seed_source, nation_id, domestic, current_team_id, active_team_id, "
            "current_league_id, age, salary, value_score, same_team_seasons, overall_value, talent_value, "
            "ratings_value, career_value, dfa, restricted, injured, updated_at"
            ") VALUES(%u, %u, %u, '%s', %u, '%s', %u, '%s', %d, '%s', %u, %d, '%s', "
            "%u, %u, %u, %u, %u, %u, %d, %d, %d, %d, %d, %d, %d, %u, %u, %u, datetime('now'));",
            row->season,
            row->team_id,
            row->date,
            escaped_source,
            row->league_id,
            team_name,
            row->player_id,
            player_name,
            row->score,
            reason,
            (uint32_t)row->seeded,
            row->seed_priority,
            seed_source,
            row->nation_id,
            (uint32_t)row->domestic,
            row->current_team_id,
            row->active_team_id,
            row->current_league_id,
            (uint32_t)row->age,
            row->salary,
            row->value_score,
            row->same_team_seasons,
            (int)row->overall_value,
            (int)row->talent_value,
            (int)row->ratings_value,
            (int)row->career_value,
            (uint32_t)row->dfa,
            (uint32_t)row->restricted,
            (uint32_t)row->injured);
    }
    if (ok) {
        ok = kbo_captain_selection_sql_append(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef("KBO captain selection sqlite replace failed reason=sql_buffer_full rows=%d", row_count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "captain_selections_replace");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}
