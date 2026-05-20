#include "custom_event_ledger.h"

#include "../../../core/sql/save_state/save_state_sqlite.h"
#include "../sql/custom_event_sql_store.h"

int kbo_custom_event_ledger_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

int kbo_custom_event_ledger_completed(uint32_t league_id, uint32_t event_yyyymmdd, KboCustomEventKind kind)
{
    return kbo_custom_event_sql_ledger_completed(league_id, event_yyyymmdd, kind);
}

void kbo_custom_event_ledger_record(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* status,
    int result,
    const char* title,
    const char* detail,
    const char* source)
{
    kbo_custom_event_sql_ledger_record(
        league_id,
        event_yyyymmdd,
        kind,
        status,
        result,
        title,
        detail,
        source);
}
