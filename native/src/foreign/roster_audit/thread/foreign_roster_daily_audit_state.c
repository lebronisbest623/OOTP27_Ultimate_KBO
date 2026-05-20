#include "../internal/foreign_roster_audit_internal.h"
#include "../state/sql/foreign_roster_audit_state_sql_store.h"

uint32_t kbo_foreign_roster_daily_load_last_audit_date(const char* source)
{
    return kbo_foreign_roster_audit_sql_load_last_audit_date(source);
}

void kbo_foreign_roster_daily_persist_last_audit_date(uint32_t today, const char* source)
{
    (void)kbo_foreign_roster_audit_sql_persist_last_audit_date(today, source);
}
