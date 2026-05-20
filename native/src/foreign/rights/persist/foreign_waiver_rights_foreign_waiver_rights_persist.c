#include "../internal/foreign_waiver_rights_internal.h"
#include "../sql/foreign_waiver_rights_sql_store.h"

/* Foreign reserve-right SQLite persistence. */

int kbo_persist_foreign_waiver_rights(void)
{
    KboForeignWaiverRetention records[KBO_FOREIGN_WAIVER_RIGHTS_MAX] = {{0}};
    int count = 0;
    kbo_lock_enter(&g_kbo_foreign_waiver_rights_lock);
    count = g_kbo_foreign_waiver_rights_count;
    if (count > KBO_FOREIGN_WAIVER_RIGHTS_MAX) {
        count = KBO_FOREIGN_WAIVER_RIGHTS_MAX;
    }
    for (int i = 0; i < count; i++) {
        records[i] = g_kbo_foreign_waiver_rights[i];
    }
    kbo_lock_leave(&g_kbo_foreign_waiver_rights_lock);

    if (!kbo_foreign_waiver_rights_sql_replace_all(records, count)) {
        kbo_log_runtime_line("foreign reserve rights: persist failed store=sqlite");
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (kbo_get_foreign_waiver_rights_path(path, sizeof(path))) {
        kbo_log_runtimef("foreign reserve rights: persisted=%d path=%s store=sqlite", count, path);
    } else {
        kbo_log_runtimef("foreign reserve rights: persisted=%d store=sqlite", count);
    }
    return 1;
}

