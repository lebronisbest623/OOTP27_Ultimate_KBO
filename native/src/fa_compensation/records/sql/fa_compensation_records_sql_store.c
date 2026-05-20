#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_compensation_records_sql_store.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../../core/dates/constants/kbo_date_constants.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboFaCompensationRecordsSqlLoadContext {
    KboFaCompensationRecord* records;
    int capacity;
    int count;
    int overflowed;
} KboFaCompensationRecordsSqlLoadContext;

static int kbo_fa_compensation_records_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS fa_compensation_records ("
        "player_id INTEGER NOT NULL,"
        "signed_on_yyyymmdd INTEGER NOT NULL DEFAULT 0,"
        "season INTEGER NOT NULL,"
        "league_id INTEGER NOT NULL DEFAULT 0,"
        "original_team_id INTEGER NOT NULL DEFAULT 0,"
        "signing_team_id INTEGER NOT NULL DEFAULT 0,"
        "grade TEXT NOT NULL DEFAULT '',"
        "previous_salary INTEGER NOT NULL DEFAULT 0,"
        "cash_with_player INTEGER NOT NULL DEFAULT 0,"
        "cash_only INTEGER NOT NULL DEFAULT 0,"
        "protect_count INTEGER NOT NULL DEFAULT 0,"
        "requires_player_compensation INTEGER NOT NULL DEFAULT 0,"
        "status INTEGER NOT NULL DEFAULT 0,"
        "case_label TEXT NOT NULL DEFAULT '',"
        "player_name TEXT NOT NULL DEFAULT '',"
        "source TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(player_id, season, original_team_id, signing_team_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_fa_compensation_records_season_status "
        "ON fa_compensation_records(season, status);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('fa_compensation_records', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "fa_compensation_records_schema");
}

int kbo_fa_compensation_records_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_fa_compensation_records_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static int32_t kbo_fa_compensation_records_sql_i32(char** vals, int index)
{
    int value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%d", &value);
    }
    return (int32_t)value;
}

static void kbo_fa_compensation_records_sql_text(char** vals, int index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (vals != NULL && vals[index] != NULL) {
        snprintf(out, out_size, "%s", vals[index]);
    }
}

static int kbo_fa_compensation_records_sql_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboFaCompensationRecordsSqlLoadContext* ctx = (KboFaCompensationRecordsSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->records == NULL || vals == NULL || ncols < 16) {
        return 0;
    }
    if (ctx->count >= ctx->capacity) {
        ctx->overflowed++;
        return 0;
    }

    KboFaCompensationRecord rec = {0};
    rec.player_id = kbo_fa_compensation_records_sql_u32(vals, 0);
    rec.signed_on_yyyymmdd = kbo_fa_compensation_records_sql_u32(vals, 1);
    rec.season = kbo_fa_compensation_records_sql_u32(vals, 2);
    rec.league_id = kbo_fa_compensation_records_sql_u32(vals, 3);
    rec.original_team_id = kbo_fa_compensation_records_sql_u32(vals, 4);
    rec.signing_team_id = kbo_fa_compensation_records_sql_u32(vals, 5);
    kbo_fa_compensation_records_sql_text(vals, 6, rec.grade, sizeof(rec.grade));
    rec.previous_salary = kbo_fa_compensation_records_sql_i32(vals, 7);
    rec.cash_with_player = kbo_fa_compensation_records_sql_u32(vals, 8);
    rec.cash_only = kbo_fa_compensation_records_sql_u32(vals, 9);
    rec.protect_count = kbo_fa_compensation_records_sql_u32(vals, 10);
    rec.requires_player_compensation = kbo_fa_compensation_records_sql_u32(vals, 11) != 0u ? 1u : 0u;
    rec.status = (uint8_t)(kbo_fa_compensation_records_sql_u32(vals, 12) & 0xffu);
    kbo_fa_compensation_records_sql_text(vals, 13, rec.case_label, sizeof(rec.case_label));
    kbo_fa_compensation_records_sql_text(vals, 14, rec.player_name, sizeof(rec.player_name));
    kbo_fa_compensation_records_sql_text(vals, 15, rec.source, sizeof(rec.source));
    if (rec.player_id == 0u || rec.season < KBO_SEASON_YEAR_MIN || rec.season > KBO_RECORD_YEAR_MAX) {
        return 0;
    }

    ctx->records[ctx->count++] = rec;
    return 0;
}

int kbo_fa_compensation_records_sql_load(KboFaCompensationRecord* records, int max_records, int* out_count)
{
    if (records == NULL || max_records <= 0 || out_count == NULL) {
        return 0;
    }
    *out_count = 0;
    if (!kbo_fa_compensation_records_sql_ensure_schema("fa_compensation_records_load_schema")) {
        return 0;
    }

    KboFaCompensationRecordsSqlLoadContext ctx = {records, max_records, 0, 0};
    static const char* sql =
        "SELECT player_id, signed_on_yyyymmdd, season, league_id, original_team_id, signing_team_id, "
        "grade, previous_salary, cash_with_player, cash_only, protect_count, "
        "requires_player_compensation, status, case_label, player_name, source "
        "FROM fa_compensation_records "
        "WHERE player_id != 0 AND season != 0 "
        "ORDER BY season, player_id, original_team_id, signing_team_id;";
    if (!kbo_save_state_query(sql, kbo_fa_compensation_records_sql_load_cb, &ctx, "fa_compensation_records_load")) {
        return 0;
    }
    if (ctx.overflowed > 0) {
        kbo_log_runtimef(
            "KBO FA compensation records sqlite load truncated rows=%d capacity=%d",
            ctx.overflowed,
            max_records);
    }
    *out_count = ctx.count;
    return 1;
}

static int kbo_fa_compensation_records_sql_append_text(
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

static int kbo_fa_compensation_records_sql_append_row(
    char* sql,
    size_t sql_size,
    size_t* cursor,
    const KboFaCompensationRecord* rec)
{
    if (rec == NULL || rec->player_id == 0u || rec->season == 0u) {
        return 1;
    }

    char grade[48] = {0};
    char case_label[128] = {0};
    char player_name[224] = {0};
    char source[128] = {0};
    if (!kbo_sql_escape_literal(grade, sizeof(grade), rec->grade)
            || !kbo_sql_escape_literal(case_label, sizeof(case_label), rec->case_label)
            || !kbo_sql_escape_literal(player_name, sizeof(player_name), rec->player_name)
            || !kbo_sql_escape_literal(source, sizeof(source), rec->source)) {
        return 0;
    }

    return kbo_fa_compensation_records_sql_append_text(
        sql,
        sql_size,
        cursor,
        "INSERT OR REPLACE INTO fa_compensation_records("
        "player_id, signed_on_yyyymmdd, season, league_id, original_team_id, signing_team_id, "
        "grade, previous_salary, cash_with_player, cash_only, protect_count, "
        "requires_player_compensation, status, case_label, player_name, source, updated_at"
        ") VALUES(%u, %u, %u, %u, %u, %u, '%s', %d, %u, %u, %u, %u, %u, '%s', '%s', '%s', datetime('now'));",
        rec->player_id,
        rec->signed_on_yyyymmdd,
        rec->season,
        rec->league_id,
        rec->original_team_id,
        rec->signing_team_id,
        grade,
        rec->previous_salary,
        rec->cash_with_player,
        rec->cash_only,
        rec->protect_count,
        (uint32_t)rec->requires_player_compensation,
        (uint32_t)rec->status,
        case_label,
        player_name,
        source);
}

int kbo_fa_compensation_records_sql_replace_all(const KboFaCompensationRecord* records, int record_count)
{
    if (record_count < 0 || (record_count > 0 && records == NULL)
            || !kbo_fa_compensation_records_sql_ensure_schema("fa_compensation_records_replace_schema")) {
        return 0;
    }

    size_t sql_size = 1024u + ((size_t)record_count * 1400u);
    char* sql = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sql_size);
    if (sql == NULL) {
        kbo_log_runtime_line("KBO FA compensation records sqlite persist failed reason=alloc_sql");
        return 0;
    }

    size_t cursor = 0u;
    int ok = kbo_fa_compensation_records_sql_append_text(
        sql,
        sql_size,
        &cursor,
        "BEGIN IMMEDIATE;DELETE FROM fa_compensation_records;");
    for (int i = 0; ok && i < record_count; i++) {
        ok = kbo_fa_compensation_records_sql_append_row(sql, sql_size, &cursor, &records[i]);
    }
    if (ok) {
        ok = kbo_fa_compensation_records_sql_append_text(sql, sql_size, &cursor, "COMMIT;");
    }
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, sql);
        kbo_log_runtimef(
            "KBO FA compensation records sqlite persist failed reason=sql_buffer_full rows=%d",
            record_count);
        return 0;
    }

    ok = kbo_save_state_exec(sql, "fa_compensation_records_replace");
    HeapFree(GetProcessHeap(), 0, sql);
    return ok;
}

int kbo_fa_compensation_records_sql_append(const KboFaCompensationRecord* rec)
{
    if (rec == NULL || rec->player_id == 0u || rec->season == 0u
            || !kbo_fa_compensation_records_sql_ensure_schema("fa_compensation_records_append_schema")) {
        return 0;
    }

    char sql[1600] = {0};
    size_t cursor = 0u;
    if (!kbo_fa_compensation_records_sql_append_row(sql, sizeof(sql), &cursor, rec)) {
        kbo_log_runtimef(
            "KBO FA compensation records sqlite append failed reason=sql_buffer_full player=%u season=%u",
            rec->player_id,
            rec->season);
        return 0;
    }
    return kbo_save_state_exec(sql, "fa_compensation_records_append");
}
