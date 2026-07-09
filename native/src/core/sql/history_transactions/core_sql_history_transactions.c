/* Core player-history and transaction SQL persistence. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_sql_history_transactions.h"
#include "core_sql_history_transactions_internal.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../logging/core_log.h"
#include "../escape/core_sql_escape.h"
#include "../league_news/core_sql_league_news.h"
#include "../save_state/save_state_sqlite.h"
#include "../text_data/core_sql_text_data_exec.h"
#include "../../dates/constants/kbo_date_constants.h"
#include "../../dates/core_text_date.h"

#define KBO_PENDING_PLAYER_HISTORY_FLUSH_MAX 64

typedef struct KboPendingPlayerHistoryRow {
    uint32_t player_id;
    uint32_t year;
    uint32_t month;
    uint32_t day;
    char history_text[2048];
    char source[128];
} KboPendingPlayerHistoryRow;

typedef struct KboPendingPlayerHistoryLoadContext {
    KboPendingPlayerHistoryRow* rows;
    int max_count;
    int count;
    int overflowed;
} KboPendingPlayerHistoryLoadContext;

static volatile LONG g_kbo_pending_player_history_flushing = 0;

static int kbo_pending_player_history_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS pending_player_history ("
        "player_id INTEGER NOT NULL,"
        "year INTEGER NOT NULL,"
        "month INTEGER NOT NULL,"
        "day INTEGER NOT NULL,"
        "history_text TEXT NOT NULL,"
        "source TEXT,"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(player_id, year, month, day, history_text)"
        ");";
    return kbo_save_state_exec(sql, source != NULL ? source : "pending_player_history_schema");
}

static uint32_t kbo_pending_history_u32(const char* value)
{
    if (value == NULL || value[0] == '\0') {
        return 0u;
    }
    return (uint32_t)strtoul(value, NULL, 10);
}

static int kbo_pending_player_history_load_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboPendingPlayerHistoryLoadContext* ctx = (KboPendingPlayerHistoryLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || ctx->max_count <= 0) {
        return 0;
    }
    if (ctx->count >= ctx->max_count) {
        ctx->overflowed = 1;
        return 0;
    }
    if (ncols < 6 || vals == NULL) {
        return 0;
    }

    KboPendingPlayerHistoryRow* row = &ctx->rows[ctx->count++];
    memset(row, 0, sizeof(*row));
    row->player_id = kbo_pending_history_u32(vals[0]);
    row->year = kbo_pending_history_u32(vals[1]);
    row->month = kbo_pending_history_u32(vals[2]);
    row->day = kbo_pending_history_u32(vals[3]);
    snprintf(row->history_text, sizeof(row->history_text), "%s", vals[4] != NULL ? vals[4] : "");
    snprintf(row->source, sizeof(row->source), "%s", vals[5] != NULL ? vals[5] : "");
    return 0;
}

static int kbo_load_pending_player_history_rows(
    KboPendingPlayerHistoryRow* rows,
    int max_count,
    int* out_count,
    const char* source)
{
    if (out_count != NULL) {
        *out_count = 0;
    }
    if (rows == NULL || max_count <= 0) {
        return 0;
    }
    if (!kbo_pending_player_history_schema(source)) {
        return 0;
    }

    KboPendingPlayerHistoryLoadContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.rows = rows;
    ctx.max_count = max_count;

    char sql[256] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT player_id, year, month, day, history_text, COALESCE(source, '') "
        "FROM pending_player_history ORDER BY created_at, player_id LIMIT %d;",
        max_count);
    if (!kbo_save_state_query(sql, kbo_pending_player_history_load_cb, &ctx, source)) {
        return 0;
    }
    if (out_count != NULL) {
        *out_count = ctx.count;
    }
    if (ctx.overflowed) {
        kbo_log_runtimef(
            "pending player history load truncated source=%s capacity=%d",
            source != NULL ? source : "",
            max_count);
    }
    return 1;
}

static int kbo_build_player_history_sql(
    char* out,
    size_t out_size,
    uint32_t player_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    const char* text)
{
    if (out == NULL || out_size == 0u || player_id == 0u || text == NULL || text[0] == '\0') {
        return 0;
    }
    out[0] = '\0';

    char escaped_text[2048] = {0};
    char history_date[16] = {0};
    if (!kbo_sql_escape_literal(escaped_text, sizeof(escaped_text), text)
            || !kbo_format_history_date(history_date, sizeof(history_date), year, month, day)) {
        return 0;
    }

    int written = snprintf(
        out,
        out_size,
        "CREATE TABLE IF NOT EXISTS player_history (history_id INTEGER PRIMARY KEY AUTOINCREMENT, player_id INTEGER, history_date VARCHAR(8), history_text TEXT, season INTEGER);"
        "DELETE FROM player_history WHERE player_id=%u AND history_date='%s' AND history_text='%s';"
        "INSERT INTO player_history(player_id, history_date, history_text, season) VALUES(%u, '%s', '%s', %u);",
        player_id,
        history_date,
        escaped_text,
        player_id,
        history_date,
        escaped_text,
        year);
    return written > 0 && (size_t)written < out_size;
}

static int kbo_delete_pending_player_history_row(const KboPendingPlayerHistoryRow* row, const char* source)
{
    if (row == NULL || row->player_id == 0u || row->history_text[0] == '\0') {
        return 0;
    }
    char escaped_text[2048] = {0};
    if (!kbo_sql_escape_literal(escaped_text, sizeof(escaped_text), row->history_text)) {
        return 0;
    }

    char sql[2600] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "DELETE FROM pending_player_history "
        "WHERE player_id=%u AND year=%u AND month=%u AND day=%u AND history_text='%s';",
        row->player_id,
        row->year,
        row->month,
        row->day,
        escaped_text);
    return kbo_save_state_exec(sql, source != NULL ? source : "pending_player_history_delete");
}

static int kbo_enqueue_pending_player_history(
    uint32_t player_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    const char* text,
    const char* source)
{
    if (player_id == 0u || text == NULL || text[0] == '\0') {
        return 0;
    }
    if (!kbo_pending_player_history_schema(source)) {
        return 0;
    }

    char escaped_text[2048] = {0};
    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_text, sizeof(escaped_text), text)
            || !kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        return 0;
    }

    char sql[3000] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR IGNORE INTO pending_player_history(player_id, year, month, day, history_text, source) "
        "VALUES(%u, %u, %u, %u, '%s', '%s');",
        player_id,
        year,
        month,
        day,
        escaped_text,
        escaped_source);
    int ok = kbo_save_state_exec(sql, source != NULL ? source : "pending_player_history_enqueue");
    kbo_log_runtimef(
        "pending player history enqueue source=%s player=%u date=%04u%02u%02u ok=%d",
        source != NULL ? source : "",
        player_id,
        year,
        month,
        day,
        ok);
    return ok;
}

int kbo_history_sqlite_exec_logged(void* database, const char* sql, const char* op, const char* source)
{
    if (sql == NULL) {
        kbo_log_runtimef(
            "history sql exec skipped source=%s op=%s reason=missing_sql",
            source != NULL ? source : "",
            op != NULL ? op : "");
        return 0;
    }

    int ok = kbo_core_sql_text_data_exec(sql, source, op);
    if (!ok) {
        kbo_log_runtimef(
            "history sql exec failed source=%s op=%s mode=text_data_only",
            source != NULL ? source : "",
            op != NULL ? op : "");
    }
    return ok;
}

static int kbo_flush_pending_player_history_sql(const char* source)
{
    if (InterlockedCompareExchange(&g_kbo_pending_player_history_flushing, 1, 0) != 0) {
        return 0;
    }

    KboPendingPlayerHistoryRow rows[KBO_PENDING_PLAYER_HISTORY_FLUSH_MAX];
    memset(rows, 0, sizeof(rows));
    int row_count = 0;
    int flushed = 0;
    int failed = 0;
    if (!kbo_load_pending_player_history_rows(
            rows,
            KBO_PENDING_PLAYER_HISTORY_FLUSH_MAX,
            &row_count,
            source)) {
        InterlockedExchange(&g_kbo_pending_player_history_flushing, 0);
        return 0;
    }

    for (int i = 0; i < row_count; i++) {
        KboPendingPlayerHistoryRow* row = &rows[i];
        char sql[5200] = {0};
        if (!kbo_build_player_history_sql(
                sql,
                sizeof(sql),
                row->player_id,
                row->year,
                row->month,
                row->day,
                row->history_text)) {
            failed++;
            continue;
        }

        const char* row_source = row->source[0] != '\0' ? row->source : source;
        if (!kbo_core_sql_text_data_exec(sql, row_source, "player_history.pending_flush")) {
            failed++;
            break;
        }
        if (kbo_delete_pending_player_history_row(row, source)) {
            flushed++;
        } else {
            failed++;
            break;
        }
    }

    if (row_count > 0) {
        kbo_log_runtimef(
            "pending player history flush source=%s loaded=%d flushed=%d failed=%d",
            source != NULL ? source : "",
            row_count,
            flushed,
            failed);
    }
    InterlockedExchange(&g_kbo_pending_player_history_flushing, 0);
    return flushed;
}

int insert_kbo_player_history_sql(
    uint32_t player_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    const char* text,
    const char* source)
{
    if (player_id == 0u || text == NULL || text[0] == '\0'
            || year < (int)KBO_HISTORY_YEAR_MIN || year > (int)KBO_RECORD_YEAR_MAX || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    char history_date[16] = {0};
    char sql[5200] = {0};
    if (!kbo_format_history_date(history_date, sizeof(history_date), year, month, day)
            || !kbo_build_player_history_sql(sql, sizeof(sql), player_id, year, month, day, text)) {
        return 0;
    }

    uintptr_t database = 0u;
    uintptr_t global = get_ootp_global_database();
    if (global != 0 && memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
    }

    int insert_result = kbo_history_sqlite_exec_logged((void*)database, sql, "player_history.upsert", source);
    int queued = insert_result ? 0 : kbo_enqueue_pending_player_history(player_id, year, month, day, text, source);
    int flushed = insert_result ? kbo_flush_pending_player_history_sql(source) : 0;
    kbo_log_runtimef(
        "player history sql insert source=%s player=%u date=%s insert=%d queued=%d flushed=%d",
        source != NULL ? source : "",
        player_id,
        history_date,
        insert_result,
        queued,
        flushed);
    return insert_result != 0 || queued != 0;
}
