#include "custom_event_sql_store.h"

#include <stdio.h>
#include <string.h>

#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboCustomEventSqlExists {
    int found;
} KboCustomEventSqlExists;

typedef struct KboCustomEventSqlKindExists {
    KboCustomEventKind kind;
    int found;
} KboCustomEventSqlKindExists;

typedef struct KboCustomEventSqlU32Result {
    uint32_t value;
    int found;
} KboCustomEventSqlU32Result;

static int kbo_custom_event_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS custom_event_markers ("
        "event_date INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "kind TEXT NOT NULL DEFAULT 'unknown',"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "PRIMARY KEY(event_date, name)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_custom_event_markers_date "
        "ON custom_event_markers(event_date);"
        "CREATE TABLE IF NOT EXISTS custom_event_runs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "event_key TEXT NOT NULL,"
        "kind TEXT NOT NULL,"
        "league_id INTEGER NOT NULL,"
        "event_date INTEGER NOT NULL,"
        "status TEXT NOT NULL,"
        "result INTEGER NOT NULL,"
        "title TEXT NOT NULL DEFAULT '',"
        "detail TEXT NOT NULL DEFAULT '',"
        "source TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_custom_event_runs_completed "
        "ON custom_event_runs(event_key, status);"
        "CREATE TABLE IF NOT EXISTS custom_event_state ("
        "key TEXT PRIMARY KEY,"
        "u32_value INTEGER NOT NULL,"
        "source TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");";
    return kbo_save_state_exec(sql, source != NULL ? source : "custom_event_sql_schema");
}

static int kbo_custom_event_sql_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)vals;
    (void)names;
    KboCustomEventSqlExists* result = (KboCustomEventSqlExists*)user_data;
    if (result != NULL) {
        result->found = 1;
    }
    return 0;
}

static int kbo_custom_event_sql_kind_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboCustomEventSqlKindExists* result = (KboCustomEventSqlKindExists*)user_data;
    if (result == NULL || result->found || ncols <= 0 || vals == NULL || vals[0] == NULL) {
        return 0;
    }
    if (kbo_custom_event_name_is_kind(vals[0], result->kind)) {
        result->found = 1;
    }
    return 0;
}

static int kbo_custom_event_sql_u32_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboCustomEventSqlU32Result* result = (KboCustomEventSqlU32Result*)user_data;
    if (result == NULL || ncols <= 0 || vals == NULL || vals[0] == NULL) {
        return 0;
    }
    unsigned int value = 0u;
    if (sscanf(vals[0], "%u", &value) == 1) {
        result->value = (uint32_t)value;
        result->found = 1;
    }
    return 0;
}

static int kbo_custom_event_sql_key(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u || event_yyyymmdd == 0u
            || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
        return 0;
    }
    int len = snprintf(
        out,
        out_size,
        "%s|%u|%u",
        kbo_custom_event_kind_key(kind),
        league_id,
        event_yyyymmdd);
    return len > 0 && len < (int)out_size;
}

int kbo_custom_event_sql_marker_exists(uint32_t event_yyyymmdd, const char* name)
{
    if (event_yyyymmdd == 0u || name == NULL || name[0] == '\0'
            || !kbo_custom_event_sql_ensure_schema("custom_event_marker_exists")) {
        return 0;
    }

    char escaped_name[256] = {0};
    if (!kbo_sql_escape_literal(escaped_name, sizeof(escaped_name), name)) {
        return 0;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM custom_event_markers "
        "WHERE event_date=%u AND name='%s' LIMIT 1;",
        event_yyyymmdd,
        escaped_name);

    KboCustomEventSqlExists result = {0};
    kbo_save_state_query(sql, kbo_custom_event_sql_exists_cb, &result, "custom_event_marker_exists");
    return result.found;
}

int kbo_custom_event_sql_marker_exists_for_kind(uint32_t event_yyyymmdd, KboCustomEventKind kind)
{
    if (event_yyyymmdd == 0u || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN || kind >= KBO_CUSTOM_EVENT_KIND_COUNT
            || !kbo_custom_event_sql_ensure_schema("custom_event_marker_exists_for_kind")) {
        return 0;
    }

    char sql[256] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT name FROM custom_event_markers WHERE event_date=%u;",
        event_yyyymmdd);

    KboCustomEventSqlKindExists result = {kind, 0};
    kbo_save_state_query(sql, kbo_custom_event_sql_kind_exists_cb, &result, "custom_event_marker_exists_for_kind");
    return result.found;
}

void kbo_custom_event_sql_marker_record(uint32_t event_yyyymmdd, const char* name, const char* source)
{
    if (event_yyyymmdd == 0u || name == NULL || name[0] == '\0'
            || !kbo_custom_event_sql_ensure_schema("custom_event_marker_record")) {
        return;
    }

    char escaped_name[256] = {0};
    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_name, sizeof(escaped_name), name)
            || !kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        kbo_log_runtimef(
            "KBO custom event marker skipped source=%s name=%s date=%u reason=escape_failed",
            source != NULL ? source : "",
            name,
            event_yyyymmdd);
        return;
    }

    KboCustomEventKind kind = kbo_custom_event_kind_from_name(name);
    const char* kind_key = kbo_custom_event_kind_key(kind);
    char sql[768] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR IGNORE INTO custom_event_markers(event_date, name, kind, source) "
        "VALUES(%u, '%s', '%s', '%s');",
        event_yyyymmdd,
        escaped_name,
        kind_key,
        escaped_source);

    if (!kbo_save_state_exec(sql, "custom_event_marker_record")) {
        kbo_log_runtimef(
            "KBO custom event marker sqlite write failed source=%s name=%s date=%u",
            source != NULL ? source : "",
            name,
            event_yyyymmdd);
    }
}

void kbo_custom_event_sql_prune_rewound_markers(const char* source)
{
    if (!kbo_custom_event_sql_ensure_schema("custom_event_marker_prune")) {
        return;
    }

    uint32_t current_date = 0u;
    if (!kbo_current_date_tick_latest_published_date(&current_date) || current_date == 0u) {
        return;
    }

    KboCustomEventSqlU32Result max_result = {0};
    kbo_save_state_query(
        "SELECT COALESCE(MAX(event_date), 0) FROM custom_event_markers;",
        kbo_custom_event_sql_u32_cb,
        &max_result,
        "custom_event_marker_prune_max");

    if (!max_result.found || max_result.value <= current_date) {
        return;
    }

    uint32_t current_year = current_date / 10000u;
    char count_sql[192] = {0};
    snprintf(
        count_sql,
        sizeof(count_sql),
        "SELECT COUNT(*) FROM custom_event_markers "
        "WHERE event_date != 0 AND (event_date / 10000) >= %u;",
        current_year);
    KboCustomEventSqlU32Result count_result = {0};
    kbo_save_state_query(
        count_sql,
        kbo_custom_event_sql_u32_cb,
        &count_result,
        "custom_event_marker_prune_count");

    char delete_sql[192] = {0};
    snprintf(
        delete_sql,
        sizeof(delete_sql),
        "DELETE FROM custom_event_markers "
        "WHERE event_date != 0 AND (event_date / 10000) >= %u;",
        current_year);
    int ok = kbo_save_state_exec(delete_sql, "custom_event_marker_prune_delete");
    kbo_log_runtimef(
        "KBO custom event marker rewind prune source=%s current=%u max_marker=%u removed=%u ok=%d store=sqlite",
        source != NULL ? source : "",
        current_date,
        max_result.value,
        count_result.value,
        ok ? 1 : 0);
}

uint32_t kbo_custom_event_sql_calendar_cursor_read(const char* source)
{
    if (!kbo_custom_event_sql_ensure_schema("custom_event_calendar_cursor_read")) {
        return 0u;
    }

    KboCustomEventSqlU32Result result = {0};
    kbo_save_state_query(
        "SELECT u32_value FROM custom_event_state WHERE key='calendar_cursor' LIMIT 1;",
        kbo_custom_event_sql_u32_cb,
        &result,
        source != NULL ? source : "custom_event_calendar_cursor_read");
    return result.found ? result.value : 0u;
}

int kbo_custom_event_sql_calendar_cursor_write(uint32_t today_yyyymmdd, const char* source)
{
    if (!kbo_custom_event_sql_ensure_schema("custom_event_calendar_cursor_write")) {
        kbo_log_runtimef(
            "KBO custom event calendar cursor skipped source=%s reason=schema_unavailable today=%u",
            source != NULL ? source : "",
            today_yyyymmdd);
        return 0;
    }

    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        return 0;
    }

    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO custom_event_state(key, u32_value, source, updated_at) "
        "VALUES('calendar_cursor', %u, '%s', datetime('now'));",
        today_yyyymmdd,
        escaped_source);
    if (!kbo_save_state_exec(sql, "custom_event_calendar_cursor_write")) {
        kbo_log_runtimef(
            "KBO custom event calendar cursor skipped source=%s reason=sqlite_write_failed today=%u",
            source != NULL ? source : "",
            today_yyyymmdd);
        return 0;
    }
    return 1;
}

int kbo_custom_event_sql_ledger_completed(uint32_t league_id, uint32_t event_yyyymmdd, KboCustomEventKind kind)
{
    char key[96] = {0};
    if (!kbo_custom_event_sql_key(league_id, event_yyyymmdd, kind, key, sizeof(key))
            || !kbo_custom_event_sql_ensure_schema("custom_event_ledger_completed")) {
        return 0;
    }

    char escaped_key[160] = {0};
    if (!kbo_sql_escape_literal(escaped_key, sizeof(escaped_key), key)) {
        return 0;
    }

    char sql[384] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM custom_event_runs "
        "WHERE event_key='%s' AND status='completed' LIMIT 1;",
        escaped_key);

    KboCustomEventSqlExists result = {0};
    kbo_save_state_query(sql, kbo_custom_event_sql_exists_cb, &result, "custom_event_ledger_completed");
    return result.found;
}

void kbo_custom_event_sql_ledger_record(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* status,
    int result,
    const char* title,
    const char* detail,
    const char* source)
{
    char key[96] = {0};
    if (!kbo_custom_event_sql_key(league_id, event_yyyymmdd, kind, key, sizeof(key))
            || status == NULL || status[0] == '\0'
            || !kbo_custom_event_sql_ensure_schema("custom_event_ledger_record")) {
        return;
    }

    char escaped_key[160] = {0};
    char escaped_status[96] = {0};
    char escaped_title[512] = {0};
    char escaped_detail[1024] = {0};
    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_key, sizeof(escaped_key), key)
            || !kbo_sql_escape_literal(escaped_status, sizeof(escaped_status), status)
            || !kbo_sql_escape_literal(escaped_title, sizeof(escaped_title), title != NULL ? title : "")
            || !kbo_sql_escape_literal(escaped_detail, sizeof(escaped_detail), detail != NULL ? detail : "")
            || !kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")) {
        kbo_log_runtimef(
            "KBO custom event ledger skipped source=%s kind=%s date=%u reason=escape_failed",
            source != NULL ? source : "",
            kbo_custom_event_kind_key(kind),
            event_yyyymmdd);
        return;
    }

    char sql[2304] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT INTO custom_event_runs("
        "event_key, kind, league_id, event_date, status, result, title, detail, source"
        ") VALUES('%s', '%s', %u, %u, '%s', %d, '%s', '%s', '%s');",
        escaped_key,
        kbo_custom_event_kind_key(kind),
        league_id,
        event_yyyymmdd,
        escaped_status,
        result,
        escaped_title,
        escaped_detail,
        escaped_source);
    if (!kbo_save_state_exec(sql, "custom_event_ledger_record")) {
        kbo_log_runtimef(
            "KBO custom event ledger sqlite write failed source=%s kind=%s date=%u",
            source != NULL ? source : "",
            kbo_custom_event_kind_key(kind),
            event_yyyymmdd);
    }
}
