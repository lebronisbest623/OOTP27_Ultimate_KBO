#ifndef KBOFIX_SRC_MILITARY_SERVICE_RUNTIME_DAYS_TICK_PLAYER_PASS_H_
#define KBOFIX_SRC_MILITARY_SERVICE_RUNTIME_DAYS_TICK_PLAYER_PASS_H_

#include <stdint.h>

typedef struct KboMilitaryDaysTickPlayerPassInput {
    const uintptr_t* player_snapshot;
    int32_t player_count;
    uint32_t vector_offset;
    uint32_t today_serial;
    const char* source;
    uint8_t* sang;
    uint8_t* kpb;
    uint32_t sang_id;
    uint32_t kpb_id;
    int source_allows_roster_mutation;
} KboMilitaryDaysTickPlayerPassInput;

typedef struct KboMilitaryDaysTickPlayerPassResult {
    int tracked;
    int monitored;
    int days_left_resynced;
    int returned;
    int newly_registered;
    int invalid_released;
    int deferred_returns;
    int deferred_invalid_releases;
    int aborted_for_save;
} KboMilitaryDaysTickPlayerPassResult;

void kbo_military_days_tick_player_pass(
    const KboMilitaryDaysTickPlayerPassInput* input,
    KboMilitaryDaysTickPlayerPassResult* result);

#endif
