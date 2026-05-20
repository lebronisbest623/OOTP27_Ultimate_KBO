#ifndef KBOFIX_SRC_FOREIGN_WAIVER_WINDOW_SQL_FOREIGN_WAIVER_WINDOW_SQL_STORE_H_
#define KBOFIX_SRC_FOREIGN_WAIVER_WINDOW_SQL_FOREIGN_WAIVER_WINDOW_SQL_STORE_H_

#include <stdint.h>

int kbo_foreign_waiver_window_sql_read(uint32_t* out_start, uint32_t* out_end);
int kbo_foreign_waiver_window_sql_write(
    uint32_t start_yyyymmdd,
    uint32_t end_yyyymmdd,
    const char* reason);

#endif
