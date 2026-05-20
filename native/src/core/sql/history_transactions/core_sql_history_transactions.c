/* Core player-history and transaction SQL persistence. */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>

#include "core_sql_history_transactions.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../logging/core_log.h"
#include "../escape/core_sql_escape.h"
#include "../league_news/core_sql_league_news.h"
#include "../../dates/constants/kbo_date_constants.h"
#include "../../dates/core_text_date.h"

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

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        kbo_log_runtimef("player history sql skipped source=%s player=%u reason=no_global", source != NULL ? source : "", player_id);
        return 0;
    }

    uintptr_t database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
    if (database == 0 || !memory_range_readable((void*)database, 0x10) || kbo_get_sqlite3_exec_fn() == NULL) {
        kbo_log_runtimef("player history sql skipped source=%s player=%u reason=db_or_exec_unavailable", source != NULL ? source : "", player_id);
        return 0;
    }

    char escaped_text[2048] = {0};
    char history_date[16] = {0};
    if (!kbo_sql_escape_literal(escaped_text, sizeof(escaped_text), text)
            || !kbo_format_history_date(history_date, sizeof(history_date), year, month, day)) {
        return 0;
    }

    char create_sql[256] = {0};
    snprintf(
        create_sql,
        sizeof(create_sql),
        "CREATE TABLE IF NOT EXISTS player_history (history_id INTEGER PRIMARY KEY AUTOINCREMENT, player_id INTEGER, history_date VARCHAR(8), history_text TEXT, season INTEGER);");

    char delete_sql[2600] = {0};
    snprintf(
        delete_sql,
        sizeof(delete_sql),
        "DELETE FROM player_history WHERE player_id=%u AND history_date='%s' AND history_text='%s';",
        player_id,
        history_date,
        escaped_text);

    char insert_sql[2600] = {0};
    snprintf(
        insert_sql,
        sizeof(insert_sql),
        "INSERT INTO player_history(player_id, history_date, history_text, season) VALUES(%u, '%s', '%s', %u);",
        player_id,
        history_date,
        escaped_text,
        year);

    int create_result = kbo_sqlite_exec_direct((void*)database, create_sql);
    int delete_result = kbo_sqlite_exec_direct((void*)database, delete_sql);
    int insert_result = kbo_sqlite_exec_direct((void*)database, insert_sql);
    kbo_log_runtimef(
        "player history sql insert source=%s player=%u date=%s create=%d delete=%d insert=%d",
        source != NULL ? source : "",
        player_id,
        history_date,
        create_result,
        delete_result,
        insert_result);
    return insert_result != 0;
}

int insert_kbo_roster_transaction_sql(
    uint32_t league_id,
    uint32_t team_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t transaction_type,
    const char* league_text,
    const char* team_text,
    const char* source)
{
    if (league_id == 0u || team_id == 0u
            || year < (int)KBO_HISTORY_YEAR_MIN || year > (int)KBO_RECORD_YEAR_MAX || month < 1 || month > 12 || day < 1 || day > 31
            || ((league_text == NULL || league_text[0] == '\0') && (team_text == NULL || team_text[0] == '\0'))) {
        return 0;
    }

    uintptr_t global = get_ootp_global_database();
    if (global == 0 || !memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        kbo_log_runtimef("roster transaction sql skipped source=%s reason=no_global", source != NULL ? source : "");
        return 0;
    }

    uintptr_t database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
    if (database == 0 || !memory_range_readable((void*)database, 0x10) || kbo_get_sqlite3_exec_fn() == NULL) {
        kbo_log_runtimef("roster transaction sql skipped source=%s reason=db_or_exec_unavailable", source != NULL ? source : "");
        return 0;
    }

    char date[16] = {0};
    char escaped_league[2048] = {0};
    char escaped_team[2048] = {0};
    if (!kbo_format_history_date(date, sizeof(date), year, month, day)
            || !kbo_sql_escape_literal(escaped_league, sizeof(escaped_league), league_text != NULL ? league_text : "")
            || !kbo_sql_escape_literal(escaped_team, sizeof(escaped_team), team_text != NULL ? team_text : "")) {
        return 0;
    }

    const char* create_league_sql =
        "CREATE TABLE IF NOT EXISTS league_transactions (transaction_id INTEGER PRIMARY KEY AUTOINCREMENT, league_id INTEGER, transaction_date VARCHAR(8), transaction_type INTEGER DEFAULT 0, transaction_text TEXT, season INTEGER);";
    const char* create_team_sql =
        "CREATE TABLE IF NOT EXISTS team_transactions (transaction_id INTEGER PRIMARY KEY AUTOINCREMENT, team_id INTEGER, transaction_date VARCHAR(8), transaction_type INTEGER DEFAULT 0, transaction_text TEXT, season INTEGER);";
    int create_league = kbo_sqlite_exec_direct((void*)database, create_league_sql);
    int create_team = kbo_sqlite_exec_direct((void*)database, create_team_sql);

    int league_insert = 0;
    int team_insert = 0;
    if (escaped_league[0] != '\0') {
        char sql[2600] = {0};
        snprintf(
            sql,
            sizeof(sql),
            "INSERT INTO league_transactions(league_id, transaction_date, transaction_type, transaction_text, season) VALUES(%u, '%s', %u, '%s', %u);",
            league_id,
            date,
            transaction_type,
            escaped_league,
            year);
        league_insert = kbo_sqlite_exec_direct((void*)database, sql);
    }
    if (escaped_team[0] != '\0') {
        char sql[2600] = {0};
        snprintf(
            sql,
            sizeof(sql),
            "INSERT INTO team_transactions(team_id, transaction_date, transaction_type, transaction_text, season) VALUES(%u, '%s', %u, '%s', %u);",
            team_id,
            date,
            transaction_type,
            escaped_team,
            year);
        team_insert = kbo_sqlite_exec_direct((void*)database, sql);
    }

    kbo_log_runtimef(
        "roster transaction sql insert source=%s date=%s league=%u team=%u create_league=%d create_team=%d league_insert=%d team_insert=%d",
        source != NULL ? source : "",
        date,
        league_id,
        team_id,
        create_league,
        create_team,
        league_insert,
        team_insert);
    return league_insert != 0 || team_insert != 0;
}
