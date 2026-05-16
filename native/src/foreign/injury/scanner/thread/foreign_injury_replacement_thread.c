#include "../foreign_injury_scanner_internal.h"

#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../common/policy/foreign_player_policy.h"

static void kbo_foreign_injury_replacement_scan_thread_catchup(
    uint32_t* last_scan_date,
    char* last_scan_save_path,
    size_t last_scan_save_path_size,
    const char* source)
{
    uint32_t today = 0u;
    char save_path[MAX_PATH] = {0};
    if (last_scan_date == NULL
            || last_scan_save_path == NULL
            || last_scan_save_path_size == 0
            || !kbo_get_current_yyyymmdd(&today)
            || !kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return;
    }

    if (last_scan_save_path[0] == '\0' || strcmp(last_scan_save_path, save_path) != 0) {
        snprintf(last_scan_save_path, last_scan_save_path_size, "%s", save_path);
        *last_scan_date = 0u;
    }
    if (today == 0u || today == *last_scan_date) {
        return;
    }

    uint32_t scan_date = today;
    if (*last_scan_date != 0u && *last_scan_date < today) {
        uint32_t next_date = kbo_add_days_yyyymmdd(*last_scan_date, 1u);
        if (next_date != 0u && next_date <= today) {
            scan_date = next_date;
        }
    }

    while (scan_date != 0u && scan_date <= today) {
        if (scan_date == today) {
            kbo_foreign_injury_replacement_scan_for_date(source, scan_date);
        } else {
            kbo_foreign_injury_replacement_scan_discovery_for_date(source, scan_date);
        }
        if (scan_date == today) {
            break;
        }
        uint32_t next_date = kbo_add_days_yyyymmdd(scan_date, 1u);
        if (next_date == 0u || next_date <= scan_date) {
            break;
        }
        scan_date = next_date;
    }

    *last_scan_date = today;
}

DWORD WINAPI kbo_foreign_injury_replacement_thread(LPVOID parameter)
{
    (void)parameter;
    uint32_t last_scan_date = 0u;
    char last_scan_save_path[MAX_PATH] = {0};
    kbo_foreign_injury_replacement_scan_thread_catchup(
        &last_scan_date,
        last_scan_save_path,
        sizeof(last_scan_save_path),
        "foreign_injury_replacement_thread_start");
    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue((uint32_t)kbo_foreign_player_policy()->injury_replacement_scan_sleep_ms)) {
            break;
        }
        kbo_foreign_injury_replacement_scan_thread_catchup(
            &last_scan_date,
            last_scan_save_path,
            sizeof(last_scan_save_path),
            "foreign_injury_replacement_thread");
    }
    InterlockedExchange(&g_kbo_foreign_injury_replacement_thread_started, 0);
    kbo_log_runtime_line("foreign injury replacement thread stopped");
    return 0;
}

void start_kbo_foreign_injury_replacement_thread(void)
{
    if (!kbo_foreign_injury_replacement_enabled()) {
        kbo_log_runtime_line("foreign injury replacement: disabled");
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_foreign_injury_replacement_thread_started, 1, 0) != 0) {
        return;
    }

    if (kbo_start_runtime_thread(
            kbo_foreign_injury_replacement_thread,
            NULL,
            "foreign injury replacement scanner")) {
        kbo_log_runtime_line("foreign injury replacement thread started");
    } else {
        InterlockedExchange(&g_kbo_foreign_injury_replacement_thread_started, 0);
    }
}
