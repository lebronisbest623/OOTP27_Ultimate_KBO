#include "../internal/foreign_waiver_rights_internal.h"
#include "../sql/foreign_waiver_rights_sql_store.h"

/* Foreign reserve-right SQLite loading. */

int kbo_load_foreign_waiver_rights(void)
{
    KboForeignWaiverRetention records[KBO_FOREIGN_WAIVER_RIGHTS_MAX] = {{0}};
    int count = 0;
    int deduped = 0;
    if (!kbo_foreign_waiver_rights_sql_load(records, KBO_FOREIGN_WAIVER_RIGHTS_MAX, &count, &deduped)) {
        return 0;
    }

    kbo_lock_enter(&g_kbo_foreign_waiver_rights_lock);
    memset(g_kbo_foreign_waiver_rights, 0, sizeof(g_kbo_foreign_waiver_rights));
    g_kbo_foreign_waiver_rights_count = 0;
    for (int i = 0; i < count && i < KBO_FOREIGN_WAIVER_RIGHTS_MAX; i++) {
        g_kbo_foreign_waiver_rights[g_kbo_foreign_waiver_rights_count++] = records[i];
    }
    InterlockedIncrement(&g_kbo_foreign_waiver_rights_generation);
    kbo_lock_leave(&g_kbo_foreign_waiver_rights_lock);

    char path[MAX_PATH] = {0};
    if (kbo_get_foreign_waiver_rights_path(path, sizeof(path))) {
        kbo_log_runtimef(
            "foreign reserve rights: loaded=%d deduped=%d path=%s store=sqlite",
            count,
            deduped,
            path);
    } else {
        kbo_log_runtimef("foreign reserve rights: loaded=%d deduped=%d store=sqlite", count, deduped);
    }
    if (deduped > 0) {
        kbo_persist_foreign_waiver_rights();
    }
    return 1;
}

