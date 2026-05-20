#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_STATE_SQL_INDEPENDENT_ACQUISITION_AI_CURSOR_SQL_STORE_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_STATE_SQL_INDEPENDENT_ACQUISITION_AI_CURSOR_SQL_STORE_H_

#include <stdint.h>

int kbo_independent_acquisition_ai_cursor_sql_load(uint32_t* out_processed_date);
int kbo_independent_acquisition_ai_cursor_sql_store(
    uint32_t processed_date,
    const char* source);

#endif
