#ifndef KBOFIX_SRC_FOREIGN_SIGNABILITY_API_FOREIGN_SIGNABILITY_SALARY_FLOOR_H_
#define KBOFIX_SRC_FOREIGN_SIGNABILITY_API_FOREIGN_SIGNABILITY_SALARY_FLOOR_H_

#include <stdint.h>

int32_t kbo_foreign_contract_salary_floor_for_player(
    uint8_t* player,
    uint32_t today,
    uint32_t* out_holder_team_id,
    int32_t* out_score,
    int* out_index,
    int* out_asian_quota);
int kbo_apply_foreign_contract_demand_floor(
    uintptr_t player_ptr,
    uint32_t today,
    const char* source);

#endif
