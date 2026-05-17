#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_SEED_STATE_AND_PATHS_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_SEED_STATE_AND_PATHS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#define KBO_ASIAN_GAMES_SCHEDULE_SEED_MAX 96

typedef struct KboAsianGamesScheduleSeed {
    uint32_t year;
    char host_city[48];
    char host_country[48];
    char status[24];
    uint32_t tournament_start;
    uint32_t tournament_end;
    uint32_t selection_date;
    uint32_t departure_date;
    uint32_t final_date;
    uint8_t auto_schedule;
    char final_result[48];
    char notes[128];
} KboAsianGamesScheduleSeed;

extern KboAsianGamesScheduleSeed g_kbo_asian_games_schedule_seeds[KBO_ASIAN_GAMES_SCHEDULE_SEED_MAX];
extern int g_kbo_asian_games_schedule_seed_count;
extern LONG g_kbo_asian_games_schedule_seed_loaded;
extern char g_kbo_asian_games_schedule_seed_loaded_key[MAX_PATH * 6];

void kbo_lock_asian_games_schedule_seeds(void);
void kbo_unlock_asian_games_schedule_seeds(void);
int kbo_get_save_asian_games_schedule_seed_path(char* out, size_t out_size);
int kbo_get_global_asian_games_schedule_seed_path(char* out, size_t out_size);

#endif
