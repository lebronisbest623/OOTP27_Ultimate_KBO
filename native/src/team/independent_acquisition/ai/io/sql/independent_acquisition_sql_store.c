#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_sql_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../../../../../core/logging/core_log.h"
#include "../../../../../core/sql/escape/core_sql_escape.h"
#include "../../../../../core/sql/save_state/save_state_sqlite.h"
#include "../../../../../core/sync/lock.h"

typedef struct KboIndependentAcquisitionSqlExistsResult {
    int found;
} KboIndependentAcquisitionSqlExistsResult;

typedef struct KboIndependentAcquisitionSqlCountResult {
    int count;
} KboIndependentAcquisitionSqlCountResult;

typedef struct KboIndependentAcquisitionSqlDateResult {
    uint32_t date;
} KboIndependentAcquisitionSqlDateResult;

typedef struct KboIndependentAcquisitionSqlQueuedLoadContext {
    KboIndependentAcquisitionQueuedRequest* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlQueuedLoadContext;

typedef struct KboIndependentAcquisitionSqlRequestLoadContext {
    KboIndependentAcquisitionSqlRequestRow* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlRequestLoadContext;

typedef struct KboIndependentAcquisitionSqlDecisionKeyLoadContext {
    KboIndependentAcquisitionDecisionKey* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlDecisionKeyLoadContext;

typedef struct KboIndependentAcquisitionSqlDecisionLoadContext {
    KboIndependentAcquisitionSqlDecisionRow* rows;
    int max_count;
    int count;
} KboIndependentAcquisitionSqlDecisionLoadContext;

static KboLock g_kbo_independent_acquisition_sql_schema_lock = KBO_LOCK_INIT;
static char g_kbo_independent_acquisition_sql_schema_path[MAX_PATH];
static int g_kbo_independent_acquisition_sql_schema_ready = 0;

static int kbo_independent_acquisition_sql_ensure_schema(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_save_state_db_path(path, sizeof(path))) {
        return 0;
    }

    kbo_lock_enter(&g_kbo_independent_acquisition_sql_schema_lock);
    if (g_kbo_independent_acquisition_sql_schema_ready
            && strcmp(g_kbo_independent_acquisition_sql_schema_path, path) == 0) {
        kbo_lock_leave(&g_kbo_independent_acquisition_sql_schema_lock);
        return 1;
    }
    kbo_lock_leave(&g_kbo_independent_acquisition_sql_schema_lock);

    static const char* sql =
        "CREATE TABLE IF NOT EXISTS independent_acquisition_requests ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date INTEGER NOT NULL,"
        "season INTEGER NOT NULL,"
        "buyer_team_id INTEGER NOT NULL,"
        "seller_team_id INTEGER NOT NULL,"
        "seller_csv_id TEXT NOT NULL DEFAULT '',"
        "player_id INTEGER NOT NULL,"
        "nation_id INTEGER NOT NULL DEFAULT 0,"
        "pitcher INTEGER NOT NULL DEFAULT 0,"
        "asian_quota INTEGER NOT NULL DEFAULT 0,"
        "cash_cost INTEGER NOT NULL DEFAULT 0,"
        "value_score INTEGER NOT NULL DEFAULT 0,"
        "request_score INTEGER NOT NULL DEFAULT 0,"
        "effective_before INTEGER NOT NULL DEFAULT 0,"
        "effective_after INTEGER NOT NULL DEFAULT 0,"
        "effective_limit INTEGER NOT NULL DEFAULT 0,"
        "slot_type TEXT NOT NULL DEFAULT '',"
        "injured_player_id INTEGER NOT NULL DEFAULT 0,"
        "buyer_active_count INTEGER NOT NULL DEFAULT 0,"
        "buyer_foreign_effective INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "UNIQUE(season, buyer_team_id, seller_team_id, player_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_independent_acquisition_requests_pending "
        "ON independent_acquisition_requests(season, seller_team_id, player_id);"
        "CREATE TABLE IF NOT EXISTS independent_acquisition_decisions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date INTEGER NOT NULL,"
        "season INTEGER NOT NULL,"
        "seller_team_id INTEGER NOT NULL,"
        "player_id INTEGER NOT NULL,"
        "buyer_team_id INTEGER NOT NULL,"
        "request_score INTEGER NOT NULL DEFAULT 0,"
        "value_score INTEGER NOT NULL DEFAULT 0,"
        "cash_cost INTEGER NOT NULL DEFAULT 0,"
        "old_cash INTEGER NOT NULL DEFAULT 0,"
        "new_cash INTEGER NOT NULL DEFAULT 0,"
        "seller_transfer_fee INTEGER NOT NULL DEFAULT 0,"
        "seller_old_cash INTEGER NOT NULL DEFAULT 0,"
        "seller_new_cash INTEGER NOT NULL DEFAULT 0,"
        "transferred INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "UNIQUE(season, seller_team_id, player_id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_independent_acquisition_decisions_buyer "
        "ON independent_acquisition_decisions(season, buyer_team_id, transferred);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('independent_acquisition_requests', 1, datetime('now'));"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('independent_acquisition_decisions', 1, datetime('now'));";
    int ok = kbo_save_state_exec(sql, source != NULL ? source : "independent_acquisition_sql_schema");
    if (!ok) {
        return 0;
    }

    kbo_lock_enter(&g_kbo_independent_acquisition_sql_schema_lock);
    snprintf(
        g_kbo_independent_acquisition_sql_schema_path,
        sizeof(g_kbo_independent_acquisition_sql_schema_path),
        "%s",
        path);
    g_kbo_independent_acquisition_sql_schema_ready = 1;
    kbo_lock_leave(&g_kbo_independent_acquisition_sql_schema_lock);
    return 1;
}

static int kbo_independent_acquisition_sql_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)vals;
    (void)names;
    KboIndependentAcquisitionSqlExistsResult* result =
        (KboIndependentAcquisitionSqlExistsResult*)user_data;
    if (result != NULL) {
        result->found = 1;
    }
    return 0;
}

static int kbo_independent_acquisition_sql_count_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)names;
    KboIndependentAcquisitionSqlCountResult* result =
        (KboIndependentAcquisitionSqlCountResult*)user_data;
    if (result != NULL && vals != NULL && vals[0] != NULL) {
        result->count = atoi(vals[0]);
    }
    return 0;
}

static int kbo_independent_acquisition_sql_date_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)names;
    KboIndependentAcquisitionSqlDateResult* result =
        (KboIndependentAcquisitionSqlDateResult*)user_data;
    if (result != NULL && vals != NULL && vals[0] != NULL) {
        unsigned int value = 0u;
        if (sscanf(vals[0], "%u", &value) == 1) {
            result->date = (uint32_t)value;
        }
    }
    return 0;
}

static uint32_t kbo_independent_acquisition_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static int32_t kbo_independent_acquisition_sql_i32(char** vals, int index)
{
    int value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%d", &value);
    }
    return (int32_t)value;
}

static int64_t kbo_independent_acquisition_sql_i64(char** vals, int index)
{
    long long value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%lld", &value);
    }
    return (int64_t)value;
}

static void kbo_independent_acquisition_sql_text(char** vals, int index, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (vals != NULL && vals[index] != NULL) {
        snprintf(out, out_size, "%s", vals[index]);
    }
}

static int kbo_independent_acquisition_sql_append_text(
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

static int kbo_independent_acquisition_sql_escape(
    char* out,
    size_t out_size,
    const char* value)
{
    return kbo_sql_escape_literal(out, out_size, value != NULL ? value : "");
}

int kbo_independent_acquisition_sql_request_exists(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || buyer_team_id == 0u || seller_team_id == 0u || player_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_request_exists_schema")) {
        return 0;
    }

    char sql[384] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM independent_acquisition_requests "
        "WHERE season=%u AND buyer_team_id=%u AND seller_team_id=%u AND player_id=%u LIMIT 1;",
        season,
        buyer_team_id,
        seller_team_id,
        player_id);
    KboIndependentAcquisitionSqlExistsResult result = {0};
    kbo_save_state_query(sql, kbo_independent_acquisition_sql_exists_cb, &result, "independent_acquisition_request_exists");
    return result.found;
}

int kbo_independent_acquisition_sql_cancel_request(
    uint32_t season,
    uint32_t buyer_team_id,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || buyer_team_id == 0u || seller_team_id == 0u || player_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_request_cancel_schema")) {
        return 0;
    }
    if (!kbo_independent_acquisition_sql_request_exists(season, buyer_team_id, seller_team_id, player_id)
            || kbo_independent_acquisition_sql_decision_exists(season, seller_team_id, player_id)) {
        return 0;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "DELETE FROM independent_acquisition_requests "
        "WHERE season=%u AND buyer_team_id=%u AND seller_team_id=%u AND player_id=%u;",
        season,
        buyer_team_id,
        seller_team_id,
        player_id);
    return kbo_save_state_exec(sql, "independent_acquisition_request_cancel");
}

int kbo_independent_acquisition_sql_append_request(
    uint32_t today,
    const KboIndependentAcquisitionCandidate* candidate,
    const KboIndependentAcquisitionBuyerState* buyer,
    const KboIndependentFuturesTeamLeague* seller,
    int32_t cash_cost,
    const char* slot_label,
    const char* source)
{
    if (today == 0u || candidate == NULL || buyer == NULL || seller == NULL
            || candidate->player_id == 0u || candidate->seller_team_id == 0u || buyer->team_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_request_append_schema")) {
        return 0;
    }

    char escaped_seller_csv_id[128] = {0};
    char escaped_slot_label[128] = {0};
    char escaped_source[512] = {0};
    if (!kbo_independent_acquisition_sql_escape(escaped_seller_csv_id, sizeof(escaped_seller_csv_id), seller->team_csv_id)
            || !kbo_independent_acquisition_sql_escape(escaped_slot_label, sizeof(escaped_slot_label), slot_label)
            || !kbo_independent_acquisition_sql_escape(escaped_source, sizeof(escaped_source), source)) {
        return 0;
    }

    char sql[4096] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR IGNORE INTO independent_acquisition_requests("
        "date, season, buyer_team_id, seller_team_id, seller_csv_id, player_id, "
        "nation_id, pitcher, asian_quota, cash_cost, value_score, request_score, "
        "effective_before, effective_after, effective_limit, slot_type, injured_player_id, "
        "buyer_active_count, buyer_foreign_effective, source"
        ") VALUES(%u, %u, %u, %u, '%s', %u, %u, %u, %u, %d, %d, %lld, %u, %u, %u, '%s', %u, %u, %u, '%s');",
        today,
        today / 10000u,
        buyer->team_id,
        candidate->seller_team_id,
        escaped_seller_csv_id,
        candidate->player_id,
        candidate->nation_id,
        (uint32_t)candidate->pitcher,
        (uint32_t)candidate->asian_quota,
        cash_cost,
        candidate->value_score,
        (long long)candidate->request_score,
        candidate->effective_before,
        candidate->effective_after,
        candidate->effective_limit,
        escaped_slot_label,
        candidate->injured_player_id,
        buyer->active_count,
        buyer->effective_foreign_count,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        kbo_log_runtime_line("independent acquisition request sqlite append skipped reason=sql_buffer_full");
        return 0;
    }
    return kbo_save_state_exec(sql, "independent_acquisition_request_append");
}

static int kbo_independent_acquisition_sql_queued_request_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboIndependentAcquisitionSqlQueuedLoadContext* ctx =
        (KboIndependentAcquisitionSqlQueuedLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 8 || ctx->count >= ctx->max_count) {
        return 0;
    }

    KboIndependentAcquisitionQueuedRequest row;
    memset(&row, 0, sizeof(row));
    row.date = kbo_independent_acquisition_sql_u32(vals, 0);
    row.season = kbo_independent_acquisition_sql_u32(vals, 1);
    row.buyer_team_id = kbo_independent_acquisition_sql_u32(vals, 2);
    row.seller_team_id = kbo_independent_acquisition_sql_u32(vals, 3);
    row.player_id = kbo_independent_acquisition_sql_u32(vals, 4);
    row.request_score = kbo_independent_acquisition_sql_i64(vals, 5);
    row.value_score = kbo_independent_acquisition_sql_i32(vals, 6);
    row.cash_cost = kbo_independent_acquisition_sql_i32(vals, 7);
    if (row.season != 0u && row.buyer_team_id != 0u && row.seller_team_id != 0u && row.player_id != 0u) {
        ctx->rows[ctx->count++] = row;
    }
    return 0;
}

int kbo_independent_acquisition_sql_load_pending_requests(
    uint32_t season,
    KboIndependentAcquisitionQueuedRequest* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_request_load_schema")) {
        return 0;
    }

    char sql[1024] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT date, season, buyer_team_id, seller_team_id, player_id, request_score, value_score, cash_cost "
        "FROM independent_acquisition_requests r "
        "WHERE r.season=%u AND NOT EXISTS("
        "SELECT 1 FROM independent_acquisition_decisions d "
        "WHERE d.season=r.season AND d.seller_team_id=r.seller_team_id AND d.player_id=r.player_id"
        ") ORDER BY date, request_score DESC, id;",
        season);
    KboIndependentAcquisitionSqlQueuedLoadContext ctx = {out, max_count, 0};
    if (!kbo_save_state_query(sql, kbo_independent_acquisition_sql_queued_request_cb, &ctx, "independent_acquisition_request_load")) {
        return 0;
    }
    return ctx.count;
}

static int kbo_independent_acquisition_sql_request_row_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboIndependentAcquisitionSqlRequestLoadContext* ctx =
        (KboIndependentAcquisitionSqlRequestLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 16 || ctx->count >= ctx->max_count) {
        return 0;
    }

    KboIndependentAcquisitionSqlRequestRow row;
    memset(&row, 0, sizeof(row));
    row.date = kbo_independent_acquisition_sql_u32(vals, 0);
    row.season = kbo_independent_acquisition_sql_u32(vals, 1);
    row.buyer_team_id = kbo_independent_acquisition_sql_u32(vals, 2);
    row.seller_team_id = kbo_independent_acquisition_sql_u32(vals, 3);
    row.player_id = kbo_independent_acquisition_sql_u32(vals, 4);
    row.nation_id = kbo_independent_acquisition_sql_u32(vals, 5);
    row.pitcher = (uint8_t)(kbo_independent_acquisition_sql_u32(vals, 6) ? 1u : 0u);
    row.asian_quota = (uint8_t)(kbo_independent_acquisition_sql_u32(vals, 7) ? 1u : 0u);
    row.cash_cost = kbo_independent_acquisition_sql_i32(vals, 8);
    row.value_score = kbo_independent_acquisition_sql_i32(vals, 9);
    row.request_score = kbo_independent_acquisition_sql_i64(vals, 10);
    row.effective_before = kbo_independent_acquisition_sql_u32(vals, 11);
    row.effective_after = kbo_independent_acquisition_sql_u32(vals, 12);
    row.effective_limit = kbo_independent_acquisition_sql_u32(vals, 13);
    kbo_independent_acquisition_sql_text(vals, 14, row.slot_type, sizeof(row.slot_type));
    row.injured_player_id = kbo_independent_acquisition_sql_u32(vals, 15);
    if (row.season != 0u && row.buyer_team_id != 0u && row.seller_team_id != 0u && row.player_id != 0u) {
        ctx->rows[ctx->count++] = row;
    }
    return 0;
}

int kbo_independent_acquisition_sql_load_request_rows(
    uint32_t season,
    uint32_t buyer_team_id,
    int pending_only,
    KboIndependentAcquisitionSqlRequestRow* out,
    int max_count)
{
    if (season == 0u || buyer_team_id == 0u || out == NULL || max_count <= 0
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_request_ui_load_schema")) {
        return 0;
    }

    char sql[1536] = {0};
    size_t cursor = 0u;
    int ok = kbo_independent_acquisition_sql_append_text(
        sql,
        sizeof(sql),
        &cursor,
        "SELECT date, season, buyer_team_id, seller_team_id, player_id, nation_id, pitcher, asian_quota, "
        "cash_cost, value_score, request_score, effective_before, effective_after, effective_limit, slot_type, injured_player_id "
        "FROM independent_acquisition_requests r WHERE r.season=%u AND r.buyer_team_id=%u ",
        season,
        buyer_team_id);
    if (ok && pending_only) {
        ok = kbo_independent_acquisition_sql_append_text(
            sql,
            sizeof(sql),
            &cursor,
            "AND NOT EXISTS(SELECT 1 FROM independent_acquisition_decisions d "
            "WHERE d.season=r.season AND d.seller_team_id=r.seller_team_id AND d.player_id=r.player_id) ");
    }
    if (ok) {
        ok = kbo_independent_acquisition_sql_append_text(
            sql,
            sizeof(sql),
            &cursor,
            "ORDER BY date DESC, request_score DESC, id DESC;");
    }
    if (!ok) {
        return 0;
    }

    KboIndependentAcquisitionSqlRequestLoadContext ctx = {out, max_count, 0};
    if (!kbo_save_state_query(sql, kbo_independent_acquisition_sql_request_row_cb, &ctx, "independent_acquisition_request_ui_load")) {
        return 0;
    }
    return ctx.count;
}

int kbo_independent_acquisition_sql_decision_exists(
    uint32_t season,
    uint32_t seller_team_id,
    uint32_t player_id)
{
    if (season == 0u || seller_team_id == 0u || player_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_decision_exists_schema")) {
        return 0;
    }

    char sql[384] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM independent_acquisition_decisions "
        "WHERE season=%u AND seller_team_id=%u AND player_id=%u LIMIT 1;",
        season,
        seller_team_id,
        player_id);
    KboIndependentAcquisitionSqlExistsResult result = {0};
    kbo_save_state_query(sql, kbo_independent_acquisition_sql_exists_cb, &result, "independent_acquisition_decision_exists");
    return result.found;
}

static int kbo_independent_acquisition_sql_decision_key_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboIndependentAcquisitionSqlDecisionKeyLoadContext* ctx =
        (KboIndependentAcquisitionSqlDecisionKeyLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 4 || ctx->count >= ctx->max_count) {
        return 0;
    }
    KboIndependentAcquisitionDecisionKey row;
    memset(&row, 0, sizeof(row));
    row.season = kbo_independent_acquisition_sql_u32(vals, 0);
    row.seller_team_id = kbo_independent_acquisition_sql_u32(vals, 1);
    row.player_id = kbo_independent_acquisition_sql_u32(vals, 2);
    row.transferred = kbo_independent_acquisition_sql_u32(vals, 3);
    if (row.season != 0u && row.seller_team_id != 0u && row.player_id != 0u) {
        ctx->rows[ctx->count++] = row;
    }
    return 0;
}

int kbo_independent_acquisition_sql_load_decision_keys(
    uint32_t season,
    KboIndependentAcquisitionDecisionKey* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_decision_keys_schema")) {
        return -1;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT season, seller_team_id, player_id, transferred "
        "FROM independent_acquisition_decisions WHERE season=%u ORDER BY id;",
        season);
    KboIndependentAcquisitionSqlDecisionKeyLoadContext ctx = {out, max_count, 0};
    if (!kbo_save_state_query(sql, kbo_independent_acquisition_sql_decision_key_cb, &ctx, "independent_acquisition_decision_keys")) {
        return -1;
    }
    return ctx.count;
}

int kbo_independent_acquisition_sql_transferred_count(
    uint32_t season,
    uint32_t team_id,
    int seller_side)
{
    if (season == 0u || team_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_decision_count_schema")) {
        return 0;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(*) FROM independent_acquisition_decisions "
        "WHERE season=%u AND %s=%u AND transferred<>0;",
        season,
        seller_side ? "seller_team_id" : "buyer_team_id",
        team_id);
    KboIndependentAcquisitionSqlCountResult result = {0};
    kbo_save_state_query(sql, kbo_independent_acquisition_sql_count_cb, &result, "independent_acquisition_decision_count");
    return result.count;
}

uint32_t kbo_independent_acquisition_sql_last_transfer_date(
    uint32_t season,
    uint32_t seller_team_id)
{
    if (season == 0u || seller_team_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_last_transfer_schema")) {
        return 0u;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT MAX(date) FROM independent_acquisition_decisions "
        "WHERE season=%u AND seller_team_id=%u AND transferred<>0;",
        season,
        seller_team_id);
    KboIndependentAcquisitionSqlDateResult result = {0};
    kbo_save_state_query(sql, kbo_independent_acquisition_sql_date_cb, &result, "independent_acquisition_last_transfer");
    return result.date;
}

int kbo_independent_acquisition_sql_append_decision(
    uint32_t today,
    const KboIndependentAcquisitionQueuedRequest* request,
    int transferred,
    int32_t old_cash,
    int32_t new_cash,
    int32_t seller_transfer_fee,
    int32_t seller_old_cash,
    int32_t seller_new_cash,
    const char* source)
{
    if (today == 0u || request == NULL
            || request->season == 0u || request->seller_team_id == 0u || request->player_id == 0u
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_decision_append_schema")) {
        return 0;
    }

    char escaped_source[512] = {0};
    if (!kbo_independent_acquisition_sql_escape(escaped_source, sizeof(escaped_source), source)) {
        return 0;
    }

    char sql[2048] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO independent_acquisition_decisions("
        "date, season, seller_team_id, player_id, buyer_team_id, request_score, value_score, cash_cost, "
        "old_cash, new_cash, seller_transfer_fee, seller_old_cash, seller_new_cash, transferred, source"
        ") VALUES(%u, %u, %u, %u, %u, %lld, %d, %d, %d, %d, %d, %d, %d, %u, '%s');",
        today,
        request->season,
        request->seller_team_id,
        request->player_id,
        request->buyer_team_id,
        (long long)request->request_score,
        request->value_score,
        request->cash_cost,
        old_cash,
        new_cash,
        seller_transfer_fee,
        seller_old_cash,
        seller_new_cash,
        transferred ? 1u : 0u,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        kbo_log_runtime_line("independent acquisition decision sqlite append skipped reason=sql_buffer_full");
        return 0;
    }
    return kbo_save_state_exec(sql, "independent_acquisition_decision_append");
}

static int kbo_independent_acquisition_sql_decision_row_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboIndependentAcquisitionSqlDecisionLoadContext* ctx =
        (KboIndependentAcquisitionSqlDecisionLoadContext*)user_data;
    if (ctx == NULL || ctx->rows == NULL || vals == NULL || ncols < 14 || ctx->count >= ctx->max_count) {
        return 0;
    }
    KboIndependentAcquisitionSqlDecisionRow row;
    memset(&row, 0, sizeof(row));
    row.date = kbo_independent_acquisition_sql_u32(vals, 0);
    row.season = kbo_independent_acquisition_sql_u32(vals, 1);
    row.seller_team_id = kbo_independent_acquisition_sql_u32(vals, 2);
    row.player_id = kbo_independent_acquisition_sql_u32(vals, 3);
    row.buyer_team_id = kbo_independent_acquisition_sql_u32(vals, 4);
    row.request_score = kbo_independent_acquisition_sql_i64(vals, 5);
    row.value_score = kbo_independent_acquisition_sql_i32(vals, 6);
    row.cash_cost = kbo_independent_acquisition_sql_i32(vals, 7);
    row.old_cash = kbo_independent_acquisition_sql_i32(vals, 8);
    row.new_cash = kbo_independent_acquisition_sql_i32(vals, 9);
    row.seller_transfer_fee = kbo_independent_acquisition_sql_i32(vals, 10);
    row.seller_old_cash = kbo_independent_acquisition_sql_i32(vals, 11);
    row.seller_new_cash = kbo_independent_acquisition_sql_i32(vals, 12);
    row.transferred = (uint8_t)(kbo_independent_acquisition_sql_u32(vals, 13) ? 1u : 0u);
    if (row.season != 0u && row.seller_team_id != 0u && row.player_id != 0u) {
        ctx->rows[ctx->count++] = row;
    }
    return 0;
}

int kbo_independent_acquisition_sql_load_decision_rows(
    uint32_t season,
    KboIndependentAcquisitionSqlDecisionRow* out,
    int max_count)
{
    if (season == 0u || out == NULL || max_count <= 0
            || !kbo_independent_acquisition_sql_ensure_schema("independent_acquisition_decision_ui_load_schema")) {
        return 0;
    }

    char sql[1024] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT date, season, seller_team_id, player_id, buyer_team_id, request_score, value_score, cash_cost, "
        "old_cash, new_cash, seller_transfer_fee, seller_old_cash, seller_new_cash, transferred "
        "FROM independent_acquisition_decisions WHERE season=%u ORDER BY date DESC, id DESC;",
        season);
    KboIndependentAcquisitionSqlDecisionLoadContext ctx = {out, max_count, 0};
    if (!kbo_save_state_query(sql, kbo_independent_acquisition_sql_decision_row_cb, &ctx, "independent_acquisition_decision_ui_load")) {
        return 0;
    }
    return ctx.count;
}
