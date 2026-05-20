#ifndef KBOFIX_SRC_FOREIGN_ROSTER_AUDIT_FOREIGN_ROSTER_AUDIT_H_
#define KBOFIX_SRC_FOREIGN_ROSTER_AUDIT_FOREIGN_ROSTER_AUDIT_H_

#include <stdint.h>

void audit_foreign_roster_state(const char* source, int write_snapshot);
uint32_t kbo_foreign_roster_daily_load_last_audit_date(const char* source);
void kbo_foreign_roster_daily_persist_last_audit_date(uint32_t today, const char* source);

#endif
