#include "foreign_waiver_announcements.h"
#include "../../core/logging/core_log.h"
#include "../common/paths/foreign_waiver_paths.h"
#include "sql/foreign_waiver_announcements_sql_store.h"

int kbo_foreign_waiver_announcement_recorded(uint32_t event_yyyymmdd)
{
    if (event_yyyymmdd == 0u) {
        return 0;
    }

    return kbo_foreign_waiver_announcements_sql_exists(event_yyyymmdd);
}

void kbo_record_foreign_waiver_announcement(uint32_t event_yyyymmdd)
{
    if (event_yyyymmdd == 0u) {
        return;
    }

    if (!kbo_foreign_waiver_announcements_sql_record(event_yyyymmdd, "", "")) {
        char path[MAX_PATH] = {0};
        get_kbo_foreign_waiver_announcement_path(path, sizeof(path));
        kbo_log_runtimef("foreign reserve rights: announcement marker record failed path=%s date=%u", path, event_yyyymmdd);
        return;
    }
}

void kbo_record_foreign_waiver_announcement_body(uint32_t event_yyyymmdd, const char* source, const char* body)
{
    if (event_yyyymmdd == 0u) {
        return;
    }

    if (!kbo_foreign_waiver_announcements_sql_record(event_yyyymmdd, source, body)) {
        char path[MAX_PATH] = {0};
        get_kbo_foreign_waiver_announcement_path(path, sizeof(path));
        kbo_log_runtimef("foreign reserve rights: announcement body record failed path=%s date=%u", path, event_yyyymmdd);
        return;
    }
}
