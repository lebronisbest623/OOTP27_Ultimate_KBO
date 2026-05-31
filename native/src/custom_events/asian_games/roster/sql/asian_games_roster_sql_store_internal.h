#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_ROSTER_SQL_ASIAN_GAMES_ROSTER_SQL_STORE_INTERNAL_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_ROSTER_SQL_ASIAN_GAMES_ROSTER_SQL_STORE_INTERNAL_H_

#include "asian_games_roster_sql_store.h"

#include <stddef.h>
#include <stdint.h>

typedef struct KboAsianGamesRosterHistorySqlLoadContext {
    KboAsianGamesRosterHistoryEntry* rows;
    int capacity;
    int count;
    int overflowed;
} KboAsianGamesRosterHistorySqlLoadContext;

typedef struct KboAsianGamesTournamentSqlLoadContext {
    KboAsianGamesTournamentHistoryEntry* rows;
    int capacity;
    int count;
    int overflowed;
} KboAsianGamesTournamentSqlLoadContext;

int kbo_asian_games_roster_sql_ensure_schema(const char* source);
uint32_t kbo_asian_games_roster_sql_u32(char** vals, int index);
int32_t kbo_asian_games_roster_sql_i32(char** vals, int index);
void kbo_asian_games_roster_sql_assign_entry(KboAsianGamesRosterEntry* entry, char** vals, int offset);
int kbo_asian_games_roster_sql_append(char* out, size_t out_size, size_t* cursor, const char* fmt, ...);
int kbo_asian_games_roster_sql_append_entry(
    char* sql,
    size_t sql_size,
    size_t* cursor,
    const char* table,
    uint32_t year,
    uint32_t slot_index,
    uint8_t result,
    const KboAsianGamesRosterEntry* entry);

#endif
