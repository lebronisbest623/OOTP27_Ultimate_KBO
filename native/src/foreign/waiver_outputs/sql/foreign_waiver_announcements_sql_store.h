#ifndef KBOFIX_SRC_FOREIGN_WAIVER_OUTPUTS_SQL_FOREIGN_WAIVER_ANNOUNCEMENTS_SQL_STORE_H_
#define KBOFIX_SRC_FOREIGN_WAIVER_OUTPUTS_SQL_FOREIGN_WAIVER_ANNOUNCEMENTS_SQL_STORE_H_

#include <stdint.h>

int kbo_foreign_waiver_announcements_sql_exists(uint32_t event_yyyymmdd);
int kbo_foreign_waiver_announcements_sql_record(
    uint32_t event_yyyymmdd,
    const char* source,
    const char* body);

#endif
