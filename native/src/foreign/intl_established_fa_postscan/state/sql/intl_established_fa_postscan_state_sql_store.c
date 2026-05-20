#include "intl_established_fa_postscan_state_sql_store.h"

#include <stdio.h>

#include "../../../../core/dates/core_text_date.h"
#include "../../../../core/sql/escape/core_sql_escape.h"
#include "../../../../core/sql/save_state/save_state_sqlite.h"

static int kbo_intl_established_fa_postscan_state_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS intl_established_fa_postscan_state ("
        "state_id INTEGER PRIMARY KEY CHECK(state_id = 1),"
        "version INTEGER NOT NULL DEFAULT 1,"
        "batch_id INTEGER NOT NULL DEFAULT 0,"
        "before_count INTEGER NOT NULL DEFAULT 0,"
        "before_max_player_id INTEGER NOT NULL DEFAULT 0,"
        "original_count INTEGER NOT NULL DEFAULT 0,"
        "expected_count INTEGER NOT NULL DEFAULT 0,"
        "multiplier INTEGER NOT NULL DEFAULT 0,"
        "primary_league_id INTEGER NOT NULL DEFAULT 0,"
        "fallback_league_id INTEGER NOT NULL DEFAULT 0,"
        "scheduled_date INTEGER NOT NULL DEFAULT 0,"
        "source TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('intl_established_fa_postscan_state', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "intl_established_fa_postscan_state_schema");
}

int kbo_intl_established_fa_postscan_state_sql_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static uint32_t kbo_intl_established_fa_postscan_state_sql_u32(char** vals, int index)
{
    unsigned int value = 0u;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%u", &value);
    }
    return (uint32_t)value;
}

static int32_t kbo_intl_established_fa_postscan_state_sql_i32(char** vals, int index)
{
    int value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%d", &value);
    }
    return (int32_t)value;
}

static long kbo_intl_established_fa_postscan_state_sql_long(char** vals, int index)
{
    long value = 0;
    if (vals != NULL && vals[index] != NULL) {
        (void)sscanf(vals[index], "%ld", &value);
    }
    return value;
}

typedef struct KboIntlEstablishedFaPostscanStateSqlLoadContext {
    KboIntlEstablishedFaPostscanState* out;
    const char* source;
    int loaded;
} KboIntlEstablishedFaPostscanStateSqlLoadContext;

static int kbo_intl_established_fa_postscan_state_sql_load_cb(
    void* user_data,
    int ncols,
    char** vals,
    char** names)
{
    (void)names;
    KboIntlEstablishedFaPostscanStateSqlLoadContext* ctx =
        (KboIntlEstablishedFaPostscanStateSqlLoadContext*)user_data;
    if (ctx == NULL || ctx->out == NULL || vals == NULL || ncols < 10) {
        return 0;
    }

    int version = kbo_intl_established_fa_postscan_state_sql_i32(vals, 0);
    long batch_id = kbo_intl_established_fa_postscan_state_sql_long(vals, 1);
    int32_t before_count = kbo_intl_established_fa_postscan_state_sql_i32(vals, 2);
    uint32_t before_max_player_id = kbo_intl_established_fa_postscan_state_sql_u32(vals, 3);
    int32_t original_count = kbo_intl_established_fa_postscan_state_sql_i32(vals, 4);
    int32_t expected_count = kbo_intl_established_fa_postscan_state_sql_i32(vals, 5);
    int multiplier = (int)kbo_intl_established_fa_postscan_state_sql_i32(vals, 6);
    uint32_t primary_league_id = kbo_intl_established_fa_postscan_state_sql_u32(vals, 7);
    uint32_t fallback_league_id = kbo_intl_established_fa_postscan_state_sql_u32(vals, 8);
    uint32_t scheduled_date = kbo_intl_established_fa_postscan_state_sql_u32(vals, 9);

    if (version != 1
            || expected_count <= 0
            || scheduled_date == 0u
            || kbo_date_serial(scheduled_date / 10000u, (scheduled_date / 100u) % 100u, scheduled_date % 100u) == 0u) {
        kbo_log_runtimef(
            "international established FA postscan state load skipped source=%s reason=invalid version=%d date=%u expected=%d",
            ctx->source != NULL ? ctx->source : "",
            version,
            scheduled_date,
            expected_count);
        return 0;
    }

    memset(ctx->out, 0, sizeof(*ctx->out));
    ctx->out->pending = 0;
    ctx->out->batch_id = (LONG)batch_id;
    ctx->out->before_count = before_count;
    ctx->out->before_max_player_id = before_max_player_id;
    ctx->out->original_count = original_count;
    ctx->out->expected_count = expected_count;
    ctx->out->multiplier = multiplier;
    ctx->out->primary_league_id = primary_league_id;
    ctx->out->fallback_league_id = fallback_league_id;
    ctx->out->scheduled_date = scheduled_date;
    ctx->out->due_tick = 0ull;
    ctx->out->attempts = 0;
    ctx->loaded = 1;
    return 0;
}

int kbo_intl_established_fa_postscan_state_sql_persist(
    const KboIntlEstablishedFaPostscanState* state,
    const char* source)
{
    if (state == NULL
            || state->scheduled_date == 0u
            || state->expected_count <= 0
            || !kbo_intl_established_fa_postscan_state_sql_ensure_schema("intl_established_fa_postscan_state_persist_schema")) {
        return 0;
    }

    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        return 0;
    }

    char sql[1536] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO intl_established_fa_postscan_state("
        "state_id, version, batch_id, before_count, before_max_player_id, "
        "original_count, expected_count, multiplier, primary_league_id, fallback_league_id, "
        "scheduled_date, source, updated_at"
        ") VALUES(1, 1, %ld, %d, %u, %d, %d, %d, %u, %u, %u, '%s', datetime('now'));",
        (long)state->batch_id,
        state->before_count,
        state->before_max_player_id,
        state->original_count,
        state->expected_count,
        state->multiplier,
        state->primary_league_id,
        state->fallback_league_id,
        state->scheduled_date,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        return 0;
    }
    return kbo_save_state_exec(sql, "intl_established_fa_postscan_state_persist");
}

int kbo_intl_established_fa_postscan_state_sql_load(
    KboIntlEstablishedFaPostscanState* out,
    const char* source)
{
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (!kbo_intl_established_fa_postscan_state_sql_ensure_schema("intl_established_fa_postscan_state_load_schema")) {
        return 0;
    }

    KboIntlEstablishedFaPostscanStateSqlLoadContext ctx = {out, source, 0};
    static const char* sql =
        "SELECT version, batch_id, before_count, before_max_player_id, "
        "original_count, expected_count, multiplier, primary_league_id, fallback_league_id, scheduled_date "
        "FROM intl_established_fa_postscan_state "
        "WHERE state_id=1 LIMIT 1;";
    if (!kbo_save_state_query(
            sql,
            kbo_intl_established_fa_postscan_state_sql_load_cb,
            &ctx,
            "intl_established_fa_postscan_state_load")) {
        return 0;
    }
    return ctx.loaded;
}
