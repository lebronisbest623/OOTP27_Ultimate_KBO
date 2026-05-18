#include "../foreign_injury_scanner_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../../../core/files/save_paths/core_save_paths.h"
#include "../../../../core/dates/tick/current_date_tick_capture.h"

#define KBO_FOREIGN_INJURY_DATE_TICK_PULSE_MS 50u
#define KBO_FOREIGN_INJURY_SQL_SETTLE_PENDING_MAX 256
#define KBO_FOREIGN_INJURY_SQL_SETTLE_ATTEMPTS 2u
#define KBO_FOREIGN_INJURY_SQL_SETTLE_FIRST_DELAY_MS 80u
#define KBO_FOREIGN_INJURY_SQL_SETTLE_RETRY_DELAY_MS 180u
#define KBO_FOREIGN_INJURY_DATE_TICK_CONSUMER_FLAGS \
    KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER

typedef struct KboForeignInjurySqlSettleDate {
    uint32_t date;
    DWORD due_tick;
    ULONGLONG last_text_data_write_time;
    uint8_t attempts;
    uint8_t has_text_data_write_time;
    uint8_t valid;
} KboForeignInjurySqlSettleDate;

static KboForeignInjurySqlSettleDate
    g_kbo_foreign_injury_sql_settle_dates[KBO_FOREIGN_INJURY_SQL_SETTLE_PENDING_MAX];

static int kbo_foreign_injury_sql_settle_due(DWORD now, DWORD due_tick)
{
    return (int32_t)(now - due_tick) >= 0;
}

static ULONGLONG kbo_foreign_injury_sql_settle_filetime_value(FILETIME filetime)
{
    return ((ULONGLONG)filetime.dwHighDateTime << 32) | (ULONGLONG)filetime.dwLowDateTime;
}

static int kbo_foreign_injury_sql_settle_path_write_time(const char* path, ULONGLONG* out_value)
{
    if (out_value != NULL) {
        *out_value = 0ull;
    }
    if (path == NULL || path[0] == '\0' || out_value == NULL) {
        return 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    memset(&attrs, 0, sizeof(attrs));
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)
            || (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
        return 0;
    }

    *out_value = kbo_foreign_injury_sql_settle_filetime_value(attrs.ftLastWriteTime);
    return *out_value != 0ull;
}

static int kbo_foreign_injury_sql_settle_text_data_write_fingerprint(ULONGLONG* out_value)
{
    if (out_value != NULL) {
        *out_value = 0ull;
    }
    if (out_value == NULL) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    char db_path[MAX_PATH] = {0};
    int written = snprintf(db_path, sizeof(db_path), "%s\\temp\\text_data.sqlite3", save_path);
    if (written <= 0 || (size_t)written >= sizeof(db_path)) {
        return 0;
    }

    ULONGLONG latest = 0ull;
    ULONGLONG value = 0ull;
    if (kbo_foreign_injury_sql_settle_path_write_time(db_path, &value) && value > latest) {
        latest = value;
    }

    char sidecar_path[MAX_PATH] = {0};
    written = snprintf(sidecar_path, sizeof(sidecar_path), "%s-wal", db_path);
    if (written > 0 && (size_t)written < sizeof(sidecar_path)
            && kbo_foreign_injury_sql_settle_path_write_time(sidecar_path, &value)
            && value > latest) {
        latest = value;
    }

    written = snprintf(sidecar_path, sizeof(sidecar_path), "%s-shm", db_path);
    if (written > 0 && (size_t)written < sizeof(sidecar_path)
            && kbo_foreign_injury_sql_settle_path_write_time(sidecar_path, &value)
            && value > latest) {
        latest = value;
    }

    if (latest == 0ull) {
        return 0;
    }
    *out_value = latest;
    return 1;
}

static void kbo_foreign_injury_schedule_sql_settle_date(uint32_t date)
{
    if (date == 0u) {
        return;
    }

    int slot = -1;
    for (int i = 0; i < KBO_FOREIGN_INJURY_SQL_SETTLE_PENDING_MAX; i++) {
        if (g_kbo_foreign_injury_sql_settle_dates[i].valid
                && g_kbo_foreign_injury_sql_settle_dates[i].date == date) {
            return;
        }
        if (!g_kbo_foreign_injury_sql_settle_dates[i].valid && slot < 0) {
            slot = i;
        }
    }
    if (slot < 0) {
        slot = 0;
    }

    g_kbo_foreign_injury_sql_settle_dates[slot].date = date;
    g_kbo_foreign_injury_sql_settle_dates[slot].due_tick =
        GetTickCount() + KBO_FOREIGN_INJURY_SQL_SETTLE_FIRST_DELAY_MS;
    g_kbo_foreign_injury_sql_settle_dates[slot].last_text_data_write_time = 0ull;
    g_kbo_foreign_injury_sql_settle_dates[slot].attempts = 0u;
    g_kbo_foreign_injury_sql_settle_dates[slot].has_text_data_write_time = 0u;
    g_kbo_foreign_injury_sql_settle_dates[slot].valid = 1u;
}

static void kbo_foreign_injury_process_sql_settle_dates(void)
{
    DWORD now = GetTickCount();
    for (int i = 0; i < KBO_FOREIGN_INJURY_SQL_SETTLE_PENDING_MAX; i++) {
        KboForeignInjurySqlSettleDate* entry = &g_kbo_foreign_injury_sql_settle_dates[i];
        if (!entry->valid || !kbo_foreign_injury_sql_settle_due(now, entry->due_tick)) {
            continue;
        }

        uint32_t date = entry->date;
        ULONGLONG text_data_write_time = 0ull;
        int have_text_data_write_time =
            kbo_foreign_injury_sql_settle_text_data_write_fingerprint(&text_data_write_time);
        if (entry->attempts > 0u
                && have_text_data_write_time
                && entry->has_text_data_write_time
                && text_data_write_time == entry->last_text_data_write_time) {
            kbo_profiler_record_us("foreign_injury.sql_settle.unchanged_text_data_skipped", 0);
            entry->valid = 0u;
            continue;
        }

        kbo_foreign_injury_sql_cache_invalidate_all("foreign_injury_current_date_tick_sql_settle");
        kbo_foreign_injury_replacement_scan_sql_settled_for_date(
            "foreign_injury_current_date_tick_sql_settle",
            date);
        if (have_text_data_write_time) {
            entry->last_text_data_write_time = text_data_write_time;
            entry->has_text_data_write_time = 1u;
        }

        entry->attempts++;
        if (entry->attempts >= KBO_FOREIGN_INJURY_SQL_SETTLE_ATTEMPTS) {
            entry->valid = 0u;
        } else {
            entry->due_tick = GetTickCount() + KBO_FOREIGN_INJURY_SQL_SETTLE_RETRY_DELAY_MS;
        }
    }
}

static int kbo_foreign_injury_date_tick_sync_consumer(
    uint32_t date,
    uint32_t site_rva,
    void* context)
{
    (void)context;
    if (!kbo_foreign_injury_replacement_enabled()) {
        return 1;
    }
    if (kbo_runtime_save_in_progress()) {
        kbo_log_runtimef(
            "foreign injury date tick sync deferred reason=save_in_progress date=%u site=0x%x",
            date,
            site_rva);
        return 0;
    }

    const char* source = site_rva == KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA
        ? "foreign_injury_current_date_tick_sync_save_enter"
        : "foreign_injury_current_date_tick_sync_post_advance";
    kbo_foreign_injury_replacement_scan_captured_date(source, date);
    kbo_foreign_injury_schedule_sql_settle_date(date);
    return !kbo_runtime_save_in_progress();
}

static int kbo_foreign_injury_date_tick_process_work(
    KboCurrentDateTickConsumer* consumer,
    const KboCurrentDateTickWork* work)
{
    if (consumer == NULL || work == NULL) {
        return 1;
    }

    kbo_current_date_tick_consumer_mark_processed(consumer);
    return 1;
}

DWORD WINAPI kbo_foreign_injury_date_tick_thread(LPVOID parameter)
{
    (void)parameter;
    kbo_log_runtime_line("foreign injury date tick thread started");

    KboCurrentDateTickConsumer consumer = {0};
    kbo_current_date_tick_consumer_init(
        &consumer,
        "foreign_injury_current_date_tick",
        KBO_FOREIGN_INJURY_DATE_TICK_CONSUMER_FLAGS);
    static volatile LONG accepted_log_count = 0;

    while (kbo_runtime_threads_should_continue()) {
        if (!kbo_runtime_sleep_should_continue(KBO_FOREIGN_INJURY_DATE_TICK_PULSE_MS)) {
            break;
        }
        if (!kbo_runtime_pause_for_save_if_needed("foreign_injury_date_tick")) {
            break;
        }

        KboCurrentDateTickWork work = {0};
        while (kbo_current_date_tick_consumer_next(&consumer, &work)) {
            LONG log_index = InterlockedIncrement(&accepted_log_count);
            if (log_index <= 80) {
                kbo_log_runtimef(
                    "foreign injury date tick event date=%u site=0x%x seq=%u missed=%u",
                    work.date,
                    work.site_rva,
                    work.sequence,
                    work.missed_events);
            }
            if (!kbo_foreign_injury_date_tick_process_work(&consumer, &work)) {
                break;
            }
        }
        kbo_foreign_injury_process_sql_settle_dates();
    }

    InterlockedExchange(&g_kbo_foreign_injury_date_tick_thread_started, 0);
    kbo_log_runtime_line("foreign injury date tick thread stopped");
    return 0;
}

void start_kbo_foreign_injury_date_tick_thread(void)
{
    if (!kbo_foreign_injury_replacement_enabled()) {
        kbo_log_runtime_line("foreign injury date tick: disabled");
        return;
    }
    if (InterlockedCompareExchange(&g_kbo_foreign_injury_date_tick_thread_started, 1, 0) != 0) {
        return;
    }
    kbo_current_date_tick_register_sync_consumer(
        "foreign_injury_current_date_tick",
        kbo_foreign_injury_date_tick_sync_consumer,
        NULL);

    if (kbo_start_runtime_thread(
            kbo_foreign_injury_date_tick_thread,
            NULL,
            "foreign injury date tick")) {
        kbo_log_runtime_line("foreign injury date tick thread requested");
    } else {
        InterlockedExchange(&g_kbo_foreign_injury_date_tick_thread_started, 0);
    }
}
