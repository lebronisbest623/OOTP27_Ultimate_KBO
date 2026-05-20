#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_ROSTER_SQL_ASIAN_GAMES_ROSTER_SQL_STORE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_ROSTER_SQL_ASIAN_GAMES_ROSTER_SQL_STORE_H_

#include "../asian_games_roster_store.h"

int kbo_asian_games_roster_sql_path(char* out, size_t out_size);
int kbo_asian_games_roster_sql_load_current(
    KboAsianGamesRosterEntry* entries,
    int max_entries,
    uint32_t* out_year,
    uint8_t* out_result,
    int* out_count);
int kbo_asian_games_roster_sql_replace_current(
    uint32_t year,
    uint8_t result,
    const KboAsianGamesRosterEntry* entries,
    int entry_count);
int kbo_asian_games_roster_sql_load_history(
    KboAsianGamesRosterHistoryEntry* out,
    int max_count,
    int* out_count);
int kbo_asian_games_roster_sql_replace_history_year(
    uint32_t year,
    uint8_t result,
    const KboAsianGamesRosterEntry* entries,
    int entry_count);
int kbo_asian_games_tournament_sql_load_history(
    KboAsianGamesTournamentHistoryEntry* out,
    int max_count,
    int* out_count);
int kbo_asian_games_tournament_sql_upsert_history(
    uint32_t year,
    uint32_t final_date,
    uint8_t result);

#endif
