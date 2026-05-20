#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core_news_ledger_sql_store.h"

#include <stdio.h>
#include <string.h>

#include "../../../logging/core_log.h"
#include "../../../sql/escape/core_sql_escape.h"
#include "../../../sql/save_state/save_state_sqlite.h"

typedef struct KboCustomNewsLedgerSqlExistsResult {
    int found;
} KboCustomNewsLedgerSqlExistsResult;

static int kbo_custom_news_ledger_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS custom_news_runs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "news_key TEXT NOT NULL,"
        "domain TEXT NOT NULL,"
        "marker TEXT NOT NULL,"
        "status TEXT NOT NULL,"
        "result INTEGER NOT NULL DEFAULT 0,"
        "title TEXT NOT NULL DEFAULT '',"
        "detail TEXT NOT NULL DEFAULT '',"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_custom_news_runs_key_status "
        "ON custom_news_runs(news_key, status);"
        "CREATE INDEX IF NOT EXISTS idx_custom_news_runs_domain_marker "
        "ON custom_news_runs(domain, marker);"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('custom_news_runs', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "custom_news_runs_schema");
}

static int kbo_custom_news_ledger_sql_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)vals;
    (void)names;
    KboCustomNewsLedgerSqlExistsResult* result = (KboCustomNewsLedgerSqlExistsResult*)user_data;
    if (result != NULL) {
        result->found = 1;
    }
    return 0;
}

static int kbo_custom_news_ledger_sql_escape_optional(
    char* out,
    size_t out_size,
    const char* value)
{
    if (kbo_sql_escape_literal(out, out_size, value != NULL ? value : "")) {
        return 1;
    }
    if (out != NULL && out_size > 0u) {
        out[0] = '\0';
    }
    return 0;
}

int kbo_custom_news_ledger_sql_completed(const char* news_key)
{
    if (news_key == NULL || news_key[0] == '\0'
            || !kbo_custom_news_ledger_sql_ensure_schema("custom_news_runs_completed_schema")) {
        return 0;
    }

    char escaped_news_key[512] = {0};
    if (!kbo_sql_escape_literal(escaped_news_key, sizeof(escaped_news_key), news_key)) {
        return 0;
    }

    char sql[768] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM custom_news_runs "
        "WHERE news_key='%s' AND status='completed' LIMIT 1;",
        escaped_news_key);
    KboCustomNewsLedgerSqlExistsResult result = {0};
    kbo_save_state_query(sql, kbo_custom_news_ledger_sql_exists_cb, &result, "custom_news_runs_completed");
    return result.found;
}

int kbo_custom_news_ledger_sql_record(
    const char* news_key,
    const char* domain,
    const char* marker,
    const char* status,
    int result,
    const char* title,
    const char* detail,
    const char* source)
{
    if (news_key == NULL || news_key[0] == '\0'
            || domain == NULL || domain[0] == '\0'
            || marker == NULL || marker[0] == '\0'
            || status == NULL || status[0] == '\0'
            || !kbo_custom_news_ledger_sql_ensure_schema("custom_news_runs_record_schema")) {
        return 0;
    }

    char escaped_news_key[512] = {0};
    char escaped_domain[512] = {0};
    char escaped_marker[512] = {0};
    char escaped_status[128] = {0};
    if (!kbo_sql_escape_literal(escaped_news_key, sizeof(escaped_news_key), news_key)
            || !kbo_sql_escape_literal(escaped_domain, sizeof(escaped_domain), domain)
            || !kbo_sql_escape_literal(escaped_marker, sizeof(escaped_marker), marker)
            || !kbo_sql_escape_literal(escaped_status, sizeof(escaped_status), status)) {
        return 0;
    }

    char escaped_title[1024] = {0};
    char escaped_detail[2048] = {0};
    char escaped_source[512] = {0};
    (void)kbo_custom_news_ledger_sql_escape_optional(escaped_title, sizeof(escaped_title), title);
    (void)kbo_custom_news_ledger_sql_escape_optional(escaped_detail, sizeof(escaped_detail), detail);
    (void)kbo_custom_news_ledger_sql_escape_optional(escaped_source, sizeof(escaped_source), source);

    char sql[8192] = {0};
    int len = snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO custom_news_runs("
        "news_key, domain, marker, status, result, title, detail, source"
        ") VALUES('%s', '%s', '%s', '%s', %d, '%s', '%s', '%s');",
        escaped_news_key,
        escaped_domain,
        escaped_marker,
        escaped_status,
        result,
        escaped_title,
        escaped_detail,
        escaped_source);
    if (len <= 0 || len >= (int)sizeof(sql)) {
        kbo_log_runtimef(
            "KBO custom news ledger sqlite record skipped domain=%s marker=%s reason=sql_buffer_full",
            domain,
            marker);
        return 0;
    }

    return kbo_save_state_exec(sql, "custom_news_runs_record");
}
