#include "foreign_waiver_announcements_sql_store.h"

#include <stdio.h>

#include "../../../core/sql/escape/core_sql_escape.h"
#include "../../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboForeignWaiverAnnouncementExistsResult {
    int found;
} KboForeignWaiverAnnouncementExistsResult;

static int kbo_foreign_waiver_announcements_sql_ensure_schema(const char* source)
{
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS foreign_waiver_announcements ("
        "event_date INTEGER PRIMARY KEY,"
        "source TEXT NOT NULL DEFAULT '',"
        "body TEXT NOT NULL DEFAULT '',"
        "created_at TEXT NOT NULL DEFAULT (datetime('now')),"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");"
        "INSERT OR REPLACE INTO kbo_schema(schema_key, schema_version, updated_at) "
        "VALUES('foreign_waiver_announcements', 1, datetime('now'));";
    return kbo_save_state_exec(sql, source != NULL ? source : "foreign_waiver_announcements_schema");
}

static int kbo_foreign_waiver_announcements_exists_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)ncols;
    (void)vals;
    (void)names;
    KboForeignWaiverAnnouncementExistsResult* result = (KboForeignWaiverAnnouncementExistsResult*)user_data;
    if (result != NULL) {
        result->found = 1;
    }
    return 0;
}

int kbo_foreign_waiver_announcements_sql_exists(uint32_t event_yyyymmdd)
{
    if (event_yyyymmdd == 0u
            || !kbo_foreign_waiver_announcements_sql_ensure_schema("foreign_waiver_announcements_exists_schema")) {
        return 0;
    }

    char sql[192] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT 1 FROM foreign_waiver_announcements WHERE event_date=%u LIMIT 1;",
        event_yyyymmdd);
    KboForeignWaiverAnnouncementExistsResult result = {0};
    kbo_save_state_query(
        sql,
        kbo_foreign_waiver_announcements_exists_cb,
        &result,
        "foreign_waiver_announcements_exists");
    return result.found;
}

int kbo_foreign_waiver_announcements_sql_record(
    uint32_t event_yyyymmdd,
    const char* source,
    const char* body)
{
    if (event_yyyymmdd == 0u
            || !kbo_foreign_waiver_announcements_sql_ensure_schema("foreign_waiver_announcements_record_schema")) {
        return 0;
    }

    char escaped_source[256] = {0};
    char escaped_body[4096] = {0};
    if (!kbo_sql_escape_literal(escaped_source, sizeof(escaped_source), source != NULL ? source : "")
            || !kbo_sql_escape_literal_preserve_ootp_controls(
                escaped_body,
                sizeof(escaped_body),
                body != NULL ? body : "")) {
        return 0;
    }

    char sql[4864] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR REPLACE INTO foreign_waiver_announcements("
        "event_date, source, body, created_at, updated_at"
        ") VALUES(%u, '%s', '%s', "
        "COALESCE((SELECT created_at FROM foreign_waiver_announcements WHERE event_date=%u), datetime('now')), "
        "datetime('now'));",
        event_yyyymmdd,
        escaped_source,
        escaped_body,
        event_yyyymmdd);
    return kbo_save_state_exec(sql, "foreign_waiver_announcements_record");
}
