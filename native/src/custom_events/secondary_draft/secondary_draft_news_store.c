#include "secondary_draft_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../core/sql/escape/core_sql_escape.h"
#include "../../core/sql/save_state/save_state_sqlite.h"

typedef struct KboSecondaryDraftNewsMarkCount {
    int found;
    int count;
} KboSecondaryDraftNewsMarkCount;

static int kbo_secondary_draft_news_mark_count_cb(void* user_data, int ncols, char** vals, char** names)
{
    (void)names;
    KboSecondaryDraftNewsMarkCount* count = (KboSecondaryDraftNewsMarkCount*)user_data;
    if (count == NULL || ncols <= 0 || vals == NULL || vals[0] == NULL) {
        return 0;
    }
    count->found = 1;
    count->count = (int)strtol(vals[0], NULL, 10);
    return 0;
}

int kbo_secondary_draft_sql_news_mark_exists(uint32_t season, const char* news_key)
{
    if (season == 0u || news_key == NULL || news_key[0] == '\0'
            || !kbo_secondary_draft_ensure_schema("secondary_draft_news_mark_exists_schema")) {
        return 0;
    }
    char escaped_key[96] = {0};
    if (!kbo_sql_escape_literal(escaped_key, sizeof(escaped_key), news_key)) {
        return 0;
    }
    char sql[320] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "SELECT COUNT(1) FROM secondary_draft_news_marks WHERE season=%u AND news_key='%s';",
        season,
        escaped_key);
    KboSecondaryDraftNewsMarkCount count = {0};
    if (!kbo_save_state_query(sql, kbo_secondary_draft_news_mark_count_cb, &count, "secondary_draft_news_mark_exists")) {
        return 0;
    }
    return count.found && count.count > 0;
}

int kbo_secondary_draft_sql_mark_news(uint32_t season, const char* news_key, const char* source)
{
    if (season == 0u || news_key == NULL || news_key[0] == '\0'
            || !kbo_secondary_draft_ensure_schema("secondary_draft_news_mark_schema")) {
        return 0;
    }
    char escaped_key[96] = {0};
    char escaped_source[256] = {0};
    if (!kbo_sql_escape_literal(escaped_key, sizeof(escaped_key), news_key)
            || !kbo_sql_escape_literal(
                escaped_source,
                sizeof(escaped_source),
                source != NULL ? source : "secondary_draft_news")) {
        return 0;
    }
    char sql[512] = {0};
    snprintf(
        sql,
        sizeof(sql),
        "INSERT OR IGNORE INTO secondary_draft_news_marks(season,news_key,source,created_at) "
        "VALUES(%u,'%s','%s',datetime('now'));",
        season,
        escaped_key,
        escaped_source);
    return kbo_save_state_exec(sql, "secondary_draft_news_mark");
}
