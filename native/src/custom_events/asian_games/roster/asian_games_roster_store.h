#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_ROSTER_STORE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_ROSTER_STORE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>
#include "../state/asian_games_state.h"

int kbo_get_asian_games_roster_csv_path(char* out, size_t out_size);
int kbo_get_asian_games_roster_history_csv_path(char* out, size_t out_size);
int kbo_get_asian_games_tournament_history_csv_path(char* out, size_t out_size);
void kbo_clear_asian_games_roster_memory(const char* source);
void kbo_clear_asian_games_roster_if_save_changed(const char* source);
int kbo_save_asian_games_roster_csv(const char* source);
int kbo_load_asian_games_roster_csv(const char* source);
int kbo_load_asian_games_roster_history(
    KboAsianGamesRosterHistoryEntry* out,
    int max_count,
    const char* source);
int kbo_append_asian_games_tournament_history(
    uint32_t year,
    uint32_t final_date,
    uint8_t result,
    const char* source);
int kbo_load_asian_games_tournament_history(
    KboAsianGamesTournamentHistoryEntry* out,
    int max_count,
    const char* source);

#endif
