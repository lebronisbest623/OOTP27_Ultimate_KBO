/* Core league news SQL persistence. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core_sql_league_news.h"
#include "core_sql_league_news_sqlite.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../logging/core_log.h"
#include "../../files/message_body/core_message_body_file.h"
#include "../../news/links/core_news_links.h"
#include "../escape/core_sql_escape.h"
#include "../../dates/core_text_date.h"
#include "../../text/ootp_text_encoding.h"

static int kbo_copy_log_string_safe(const char* source, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (source == NULL || !memory_range_readable(source, 1)) {
        return 0;
    }

    size_t len = 0;
    for (; len + 1 < out_size && len < 256u; len++) {
        if (!memory_range_readable(source + len, 1)) {
            break;
        }
        char c = source[len];
        if (c == '\0') {
            break;
        }
        if ((unsigned char)c < 0x20u) {
            c = ' ';
        }
        out[len] = c;
    }
    out[len] = '\0';
    return len > 0;
}

static uint32_t kbo_league_news_stable_message_id(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    const char* title)
{
    uint32_t hash = 2166136261u;
    hash ^= league_id;
    hash *= 16777619u;
    if (title != NULL) {
        for (size_t i = 0; title[i] != '\0' && i < 512u; i++) {
            hash ^= (uint8_t)title[i];
            hash *= 16777619u;
        }
    }

    return 900000000u
        + ((year % 100u) * 10000000u)
        + (month * 100000u)
        + (day * 1000u)
        + (hash % 997u);
}

int insert_kbo_league_news_sql(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body,
    const char* source)
{
    if (title == NULL || title[0] == '\0' || league_id == 0
            || year < 1800 || year > 2300 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    char log_title[192] = {0};
    char log_source[96] = {0};
    kbo_copy_log_string_safe(title, log_title, sizeof(log_title));
    kbo_copy_log_string_safe(source != NULL ? source : "", log_source, sizeof(log_source));

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)global, OOTP27_GLOBAL_SQL_ENABLED_OFFSET + sizeof(uint8_t))) {
        kbo_log_runtimef("league news sql skipped source=%s title=%s reason=no_global_database", log_source, log_title);
        return 0;
    }

    uint8_t sql_enabled = *(uint8_t*)(global + OOTP27_GLOBAL_SQL_ENABLED_OFFSET);
    if (sql_enabled == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        kbo_log_runtimef(
            "league news sql skipped source=%s title=%s reason=sql_disabled_or_unreadable enabled=%u",
            log_source,
            log_title,
            (uint32_t)sql_enabled);
        return 0;
    }

    uintptr_t database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
    if (database == 0 || !memory_range_readable((void*)database, 8)) {
        kbo_log_runtimef("league news sql skipped source=%s title=%s reason=no_sql_database db=%p", log_source, log_title, (void*)database);
        return 0;
    }

    KboSqlite3ExecFn sqlite_exec = kbo_get_sqlite3_exec_fn();
    if (sqlite_exec == NULL) {
        kbo_log_runtimef("league news sql skipped source=%s title=%s reason=sqlite3_exec_unavailable", log_source, log_title);
        return 0;
    }

    char escaped_title[512] = {0};
    char escaped_body[8192] = {0};
    char news_date[32] = {0};
    char display_title[512] = {0};
    const char* body_text = body != NULL ? body : "";
    kbo_news_strip_link_markup(title, display_title, sizeof(display_title));
    const char* title_text = display_title[0] != '\0' ? display_title : title;
    char* internal_title = kbo_alloc_ootp_internal_text(title_text);
    char* internal_body = kbo_alloc_ootp_internal_text(body_text);
    const char* title_for_ootp = internal_title != NULL ? internal_title : title_text;
    const char* body_for_ootp = internal_body != NULL ? internal_body : body_text;
    int uses_internal_encoding = internal_title != NULL || internal_body != NULL;
    if (!kbo_sql_escape_literal_preserve_ootp_controls(escaped_title, sizeof(escaped_title), title_for_ootp)
            || !kbo_sql_escape_literal_preserve_ootp_controls(escaped_body, sizeof(escaped_body), body_for_ootp)
            || !kbo_format_history_date(news_date, sizeof(news_date), year, month, day)) {
        kbo_log_runtimef("league news sql skipped source=%s title=%s reason=escape_or_date_failed", log_source, log_title);
        kbo_free_ootp_internal_text(internal_title);
        kbo_free_ootp_internal_text(internal_body);
        return 0;
    }

    uint32_t message_id = kbo_league_news_stable_message_id(year, month, day, league_id, title_text);
    KboNewsRelatedIds related;
    kbo_news_related_ids_collect_pair(&related, title, body);

    char escaped_raw_title[512] = {0};
    char* internal_raw_title = NULL;
    int has_raw_title_delete = strcmp(title_text, title) != 0;
    if (has_raw_title_delete) {
        internal_raw_title = kbo_alloc_ootp_internal_text(title);
        const char* raw_title_for_ootp = internal_raw_title != NULL ? internal_raw_title : title;
        has_raw_title_delete = kbo_sql_escape_literal_preserve_ootp_controls(
            escaped_raw_title,
            sizeof(escaped_raw_title),
            raw_title_for_ootp);
    }

    char delete_sql[1400] = {0};
    if (has_raw_title_delete) {
        snprintf(
            delete_sql,
            sizeof(delete_sql),
            "DELETE FROM messages WHERE message_id=%u OR ((subject='%s' OR subject='%s') AND date='%s' AND league_id_0=%u AND sender_type=9001);",
            message_id,
            escaped_title,
            escaped_raw_title,
            news_date,
            league_id);
    } else {
        snprintf(
            delete_sql,
            sizeof(delete_sql),
            "DELETE FROM messages WHERE message_id=%u OR (subject='%s' AND date='%s' AND league_id_0=%u AND sender_type=9001);",
            message_id,
            escaped_title,
            news_date,
            league_id);
    }

    char insert_sql[12000] = {0};
    snprintf(
        insert_sql,
        sizeof(insert_sql),
        "INSERT INTO messages(message_id, subject, player_id_0, player_id_1, player_id_2, player_id_3, player_id_4, team_id_0, team_id_1, team_id_2, team_id_3, team_id_4, league_id_0, league_id_1, importance, message_type, hype, sender_type, sender_id, recipient_id, trade_id, date, deleted, notify, ongoing_story_id, text_is_modified, body) "
        "VALUES(%u, '%s', %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, 0, 3, %u, 0, 9001, %u, 0, 0, '%s', 0, 1, 0, 1, '%s');",
        message_id,
        escaped_title,
        related.player_ids[0],
        related.player_ids[1],
        related.player_ids[2],
        related.player_ids[3],
        related.player_ids[4],
        related.team_ids[0],
        related.team_ids[1],
        related.team_ids[2],
        related.team_ids[3],
        related.team_ids[4],
        league_id,
        message_type,
        league_id,
        news_date,
        escaped_body);

    int delete_result = sqlite_exec((void*)database, delete_sql, NULL, NULL, NULL) == 0 ? 1 : 0;
    int insert_result = sqlite_exec((void*)database, insert_sql, NULL, NULL, NULL) == 0 ? 1 : 0;
    int body_file = 0;
    if (insert_result != 0) {
        body_file = write_kbo_message_body_file(message_id, title_text, body, source);
    }
    kbo_log_runtimef(
        "league news sql insert source=%s title=%s date=%s league_id=%u type=%u message_id=%u related_players=%d related_teams=%d delete_result=%d insert_result=%d body_file=%d db=%p encoding=%s",
        source != NULL ? source : "",
        title,
        news_date,
        league_id,
        message_type,
        message_id,
        related.player_count,
        related.team_count,
        delete_result,
        insert_result,
        body_file,
        (void*)database,
        uses_internal_encoding ? "ootp-internal" : "raw");
    kbo_free_ootp_internal_text(internal_raw_title);
    kbo_free_ootp_internal_text(internal_title);
    kbo_free_ootp_internal_text(internal_body);

    return insert_result != 0;
}

int insert_kbo_league_news_table_sql(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    const char* title,
    const char* body,
    const char* source)
{
    if (title == NULL || title[0] == '\0' || league_id == 0
            || year < 1800 || year > 2300 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    char log_title[192] = {0};
    char log_source[96] = {0};
    kbo_copy_log_string_safe(title, log_title, sizeof(log_title));
    kbo_copy_log_string_safe(source != NULL ? source : "", log_source, sizeof(log_source));

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        kbo_log_runtimef("league_news table sql skipped source=%s title=%s reason=no_global", log_source, log_title);
        return 0;
    }

    uintptr_t database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
    if (database == 0 || !memory_range_readable((void*)database, 0x10)) {
        kbo_log_runtimef("league_news table sql skipped source=%s title=%s reason=no_database db=%p", log_source, log_title, (void*)database);
        return 0;
    }

    KboSqlite3ExecFn sqlite_exec = kbo_get_sqlite3_exec_fn();
    if (sqlite_exec == NULL) {
        kbo_log_runtimef("league_news table sql skipped source=%s title=%s reason=sqlite3_exec_unavailable", log_source, log_title);
        return 0;
    }

    char escaped_text[8192] = {0};
    char escaped_title[512] = {0};
    char escaped_body[7600] = {0};
    char news_date[16] = {0};
    char display_title[512] = {0};
    const char* body_text = body != NULL ? body : "";
    kbo_news_strip_link_markup(title, display_title, sizeof(display_title));
    const char* title_text = display_title[0] != '\0' ? display_title : title;
    char* internal_title = kbo_alloc_ootp_internal_text(title_text);
    char* internal_body = kbo_alloc_ootp_internal_text(body_text);
    const char* title_for_ootp = internal_title != NULL ? internal_title : title_text;
    const char* body_for_ootp = internal_body != NULL ? internal_body : body_text;
    int uses_internal_encoding = internal_title != NULL || internal_body != NULL;
    if (!kbo_sql_escape_literal_preserve_ootp_controls(escaped_title, sizeof(escaped_title), title_for_ootp)
            || !kbo_sql_escape_literal_preserve_ootp_controls(escaped_body, sizeof(escaped_body), body_for_ootp)
            || !kbo_format_history_date(news_date, sizeof(news_date), year, month, day)) {
        kbo_log_runtimef("league_news table sql skipped source=%s title=%s reason=escape_or_date_failed", log_source, log_title);
        kbo_free_ootp_internal_text(internal_title);
        kbo_free_ootp_internal_text(internal_body);
        return 0;
    }

    snprintf(
        escaped_text,
        sizeof(escaped_text),
        "%s%s%s",
        escaped_title,
        escaped_body[0] != '\0' ? "\n\n" : "",
        escaped_body);

    char escaped_raw_title[512] = {0};
    char escaped_raw_text[8192] = {0};
    char* internal_raw_title = NULL;
    int has_raw_text_delete = strcmp(title_text, title) != 0;
    if (has_raw_text_delete) {
        internal_raw_title = kbo_alloc_ootp_internal_text(title);
        const char* raw_title_for_ootp = internal_raw_title != NULL ? internal_raw_title : title;
        has_raw_text_delete = kbo_sql_escape_literal_preserve_ootp_controls(
            escaped_raw_title,
            sizeof(escaped_raw_title),
            raw_title_for_ootp);
        if (has_raw_text_delete) {
            snprintf(
                escaped_raw_text,
                sizeof(escaped_raw_text),
                "%s%s%s",
                escaped_raw_title,
                escaped_body[0] != '\0' ? "\n\n" : "",
                escaped_body);
        }
    }

    const char* create_sql =
        "CREATE TABLE IF NOT EXISTS league_news (news_id INTEGER PRIMARY KEY AUTOINCREMENT, league_id INTEGER, news_date VARCHAR(8), news_text TEXT, season INTEGER);"
        "CREATE INDEX IF NOT EXISTS league_news_league_id ON league_news(league_id);"
        "CREATE INDEX IF NOT EXISTS league_news_date ON league_news(news_date);";

    char delete_sql[18000] = {0};
    if (has_raw_text_delete) {
        snprintf(
            delete_sql,
            sizeof(delete_sql),
            "DELETE FROM league_news WHERE league_id=%u AND news_date='%s' AND (news_text='%s' OR news_text='%s');",
            league_id,
            news_date,
            escaped_text,
            escaped_raw_text);
    } else {
        snprintf(
            delete_sql,
            sizeof(delete_sql),
            "DELETE FROM league_news WHERE league_id=%u AND news_date='%s' AND news_text='%s';",
            league_id,
            news_date,
            escaped_text);
    }

    char insert_sql[10000] = {0};
    snprintf(
        insert_sql,
        sizeof(insert_sql),
        "INSERT INTO league_news(league_id, news_date, news_text, season) VALUES(%u, '%s', '%s', %u);",
        league_id,
        news_date,
        escaped_text,
        year);

    int create_result = sqlite_exec((void*)database, create_sql, NULL, NULL, NULL) == 0 ? 1 : 0;
    int delete_result = sqlite_exec((void*)database, delete_sql, NULL, NULL, NULL) == 0 ? 1 : 0;
    int insert_result = sqlite_exec((void*)database, insert_sql, NULL, NULL, NULL) == 0 ? 1 : 0;
    int fallback_result = 0;
    if (insert_result == 0) {
        fallback_result = kbo_insert_league_news_table_text_data_fallback(
            create_sql,
            delete_sql,
            insert_sql,
            source,
            title,
            news_date,
            league_id);
    }
    kbo_log_runtimef(
        "league_news table sql insert source=%s title=%s date=%s league_id=%u create=%d delete=%d insert=%d fallback=%d db=%p exec=%p encoding=%s",
        source != NULL ? source : "",
        title,
        news_date,
        league_id,
        create_result,
        delete_result,
        insert_result,
        fallback_result,
        (void*)database,
        (void*)sqlite_exec,
        uses_internal_encoding ? "ootp-internal" : "raw");
    kbo_free_ootp_internal_text(internal_raw_title);
    kbo_free_ootp_internal_text(internal_title);
    kbo_free_ootp_internal_text(internal_body);
    return insert_result != 0 || fallback_result != 0;
}
