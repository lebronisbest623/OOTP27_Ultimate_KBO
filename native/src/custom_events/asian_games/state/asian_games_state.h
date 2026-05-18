#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_STATE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_STATE_H_

#include <stdint.h>
#include <windows.h>

#define KBO_ASIAN_GAMES_ROSTER_SIZE 24
#define KBO_ASIAN_GAMES_MAX_CANDIDATES 4096
#define KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES 8
#define KBO_ASIAN_GAMES_MAX_ORGS 32
#define KBO_ASIAN_GAMES_RESULT_UNKNOWN 0
#define KBO_ASIAN_GAMES_RESULT_GOLD 1
#define KBO_ASIAN_GAMES_RESULT_NO_GOLD 2

typedef struct KboAsianGamesRosterEntry {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint32_t departure_date;
    uint32_t return_date;
    uint16_t age;
    uint8_t role;
    uint8_t wildcard;
    uint8_t military_unserved;
    uint8_t old_restricted;
    uint8_t old_secondary_restricted;
    uint8_t old_injury_active;
    int16_t old_injury_days_left;
    uint8_t departed;
    uint8_t returned;
    uint8_t exempted;
    int32_t score;
    uintptr_t player_ptr;
} KboAsianGamesRosterEntry;

typedef struct KboAsianGamesCandidate {
    KboAsianGamesRosterEntry entry;
    uint32_t org_team_id;
    uint8_t selected;
} KboAsianGamesCandidate;

typedef struct KboAsianGamesTournamentHistoryEntry {
    uint32_t year;
    uint32_t final_date;
    uint8_t result;
} KboAsianGamesTournamentHistoryEntry;

typedef struct KboAsianGamesRosterHistoryEntry {
    uint32_t year;
    uint32_t index;
    KboAsianGamesRosterEntry entry;
    uint8_t tournament_result;
} KboAsianGamesRosterHistoryEntry;

extern KboAsianGamesRosterEntry g_kbo_asian_games_roster[KBO_ASIAN_GAMES_ROSTER_SIZE];
extern LONG g_kbo_asian_games_roster_count;
extern uint32_t g_kbo_asian_games_roster_year;
extern uint8_t g_kbo_asian_games_result;
extern char g_kbo_asian_games_roster_save_path[MAX_PATH];

#endif
