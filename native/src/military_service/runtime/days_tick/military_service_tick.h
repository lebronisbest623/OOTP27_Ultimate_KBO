#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_TICK_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_TICK_H_

#include <stdint.h>

int kbo_tick_military_service_days_for_date(
    uint32_t today_yyyymmdd,
    const char* source,
    int* out_seeded_assignments);

#endif
