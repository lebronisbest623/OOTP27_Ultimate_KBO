#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_WINDOW_SQL_INDEPENDENT_ACQUISITION_WINDOW_SQL_STORE_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_WINDOW_SQL_INDEPENDENT_ACQUISITION_WINDOW_SQL_STORE_H_

#include <stdint.h>

int kbo_independent_acquisition_window_sql_load_open_date(uint32_t* out_open_date);
int kbo_independent_acquisition_window_sql_store_open_date(
    uint32_t open_date,
    const char* source);

#endif
