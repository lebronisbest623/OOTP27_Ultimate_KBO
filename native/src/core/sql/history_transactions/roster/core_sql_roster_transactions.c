#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../core_sql_history_transactions.h"
#include "../core_sql_history_transactions_internal.h"

#include <stdint.h>
#include <stdio.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../dates/constants/kbo_date_constants.h"
#include "../../../dates/core_text_date.h"
#include "../../../logging/core_log.h"
#include "../../escape/core_sql_escape.h"

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

    uintptr_t database = 0u;
    uintptr_t global = get_ootp_global_database();
    if (global != 0 && memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
        database = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
    }

    int create_league = kbo_history_sqlite_exec_logged((void*)database, create_league_sql, "league_transactions.create", source);
    int create_team = kbo_history_sqlite_exec_logged((void*)database, create_team_sql, "team_transactions.create", source);

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
        league_insert = kbo_history_sqlite_exec_logged((void*)database, sql, "league_transactions.insert", source);
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
        team_insert = kbo_history_sqlite_exec_logged((void*)database, sql, "team_transactions.insert", source);
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
