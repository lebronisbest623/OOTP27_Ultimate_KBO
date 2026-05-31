#ifndef KBOFIX_SRC_SERVICE_TIME_GUARD_H_
#define KBOFIX_SRC_SERVICE_TIME_GUARD_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

typedef struct KboServiceTimeGuardApplyResult {
    int protected_player;
    int baseline_registered;
    int service_time_restored;
    uint32_t protected_team_id;
    uint32_t service_time_before;
    uint32_t baseline_service_time;
} KboServiceTimeGuardApplyResult;

uint32_t kbo_service_time_guard_player_days(uint8_t* player);
int kbo_service_time_guard_set_player_days(uint8_t* player, uint32_t service_days);
int kbo_service_time_guard_team_excluded(uint32_t team_id);
int kbo_service_time_guard_apply_player(
    uint8_t* player,
    uint32_t date_yyyymmdd,
    const char* source,
    KboServiceTimeGuardApplyResult* out_result);
void kbo_service_time_guard_reset_for_tests(void);
DWORD WINAPI kbo_service_time_guard_thread(LPVOID parameter);
void start_kbo_service_time_guard_thread(void);

#endif
