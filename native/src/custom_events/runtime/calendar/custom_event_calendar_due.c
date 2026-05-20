#include "custom_event_calendar_due.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "../../../competitive_balance_tax/events/cbt_events.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../asian_games_lifecycle/maintenance/asian_games_lifecycle_maintenance.h"
#include "../../asian_games/schedule/asian_games_schedule.h"
#include "../../schedules/independent/independent_team_acquisition_schedule.h"
#include "../../schedules/priority/foreign_priority_event_schedule.h"
#include "../scan/custom_event_scan.h"

#define KBO_CUSTOM_EVENT_SCAN_MAX_ATTEMPTS 32
#define KBO_CUSTOM_EVENT_IDLE_LOG_INITIAL_BURST 20
#define KBO_CUSTOM_EVENT_IDLE_LOG_PERIOD 200
#define KBO_CUSTOM_EVENT_BUSY_LOG_THROTTLE_MS 30000ull

typedef struct KboCustomEventDueResults {
    int foreign;
    int asian;
    int asian_hold;
    int cbt;
    int independent;
    int scanned;
} KboCustomEventDueResults;

static int kbo_custom_event_calendar_cursor_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("custom_event_calendar_cursor.txt", out, out_size);
}

static volatile LONG g_kbo_custom_event_calendar_cursor_cached_initialized = 0;
static volatile LONG g_kbo_custom_event_calendar_cursor_cached_value = 0;
static volatile LONG g_kbo_custom_event_calendar_due_processing = 0;
static volatile LONG64 g_kbo_custom_event_calendar_due_busy_log_ms = 0;
static char g_kbo_custom_event_calendar_cursor_cached_path[MAX_PATH] = {0};

static void kbo_custom_event_calendar_cache_cursor(const char* path, uint32_t cursor)
{
    if (path == NULL) {
        return;
    }
    snprintf(g_kbo_custom_event_calendar_cursor_cached_path, sizeof(g_kbo_custom_event_calendar_cursor_cached_path), "%s", path);
    InterlockedExchange(&g_kbo_custom_event_calendar_cursor_cached_value, (LONG)cursor);
    InterlockedExchange(&g_kbo_custom_event_calendar_cursor_cached_initialized, 1);
}

static uint32_t kbo_custom_event_calendar_read_cursor(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_custom_event_calendar_cursor_path(path, sizeof(path))) {
        return 0u;
    }

    if (InterlockedCompareExchange(&g_kbo_custom_event_calendar_cursor_cached_initialized, 0, 0) != 0
            && strcmp(g_kbo_custom_event_calendar_cursor_cached_path, path) == 0) {
        return (uint32_t)InterlockedCompareExchange(&g_kbo_custom_event_calendar_cursor_cached_value, 0, 0);
    }

    FILE* file = fopen(path, "r");
    if (file == NULL) {
        kbo_custom_event_calendar_cache_cursor(path, 0u);
        return 0u;
    }

    unsigned int value = 0u;
    int matched = fscanf(file, "%u", &value);
    fclose(file);
    uint32_t cursor = matched == 1 ? (uint32_t)value : 0u;
    kbo_custom_event_calendar_cache_cursor(path, cursor);
    return cursor;
}

static void kbo_custom_event_calendar_write_cursor(uint32_t today_yyyymmdd, const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_custom_event_calendar_cursor_path(path, sizeof(path))) {
        kbo_log_runtimef(
            "KBO custom event calendar cursor skipped source=%s reason=path_unavailable today=%u",
            source != NULL ? source : "",
            today_yyyymmdd);
        return;
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "KBO custom event calendar cursor skipped source=%s reason=open_failed today=%u path=%s gle=%lu",
            source != NULL ? source : "",
            today_yyyymmdd,
            path,
            GetLastError());
        return;
    }

    char line[16] = {0};
    int len = snprintf(line, sizeof(line), "%u\n", today_yyyymmdd);
    DWORD written = 0;
    int ok = len > 0
        && WriteFile(file, line, (DWORD)len, &written, NULL)
        && written == (DWORD)len;
    if (!ok) {
        kbo_atomic_abort(file, tmp_path);
        kbo_log_runtimef(
            "KBO custom event calendar cursor skipped source=%s reason=write_failed today=%u path=%s gle=%lu",
            source != NULL ? source : "",
            today_yyyymmdd,
            path,
            GetLastError());
        return;
    }
    if (!kbo_atomic_commit(file, tmp_path, path)) {
        kbo_log_runtimef(
            "KBO custom event calendar cursor skipped source=%s reason=commit_failed today=%u path=%s gle=%lu",
            source != NULL ? source : "",
            today_yyyymmdd,
            path,
            GetLastError());
        return;
    }
    kbo_custom_event_calendar_cache_cursor(path, today_yyyymmdd);
}

static int kbo_custom_event_calendar_should_log_idle_due(
    int changed,
    int deferred,
    int schedule_blocked)
{
    if (changed || deferred || schedule_blocked) {
        return 1;
    }
    static volatile LONG idle_log_count = 0;
    LONG slot = InterlockedIncrement(&idle_log_count);
    return slot <= KBO_CUSTOM_EVENT_IDLE_LOG_INITIAL_BURST
        || (slot % KBO_CUSTOM_EVENT_IDLE_LOG_PERIOD) == 0;
}

static int kbo_custom_event_calendar_scan_until_idle(uint32_t today_yyyymmdd, const char* source)
{
    int total_triggered = 0;
    for (int attempt = 0; attempt < KBO_CUSTOM_EVENT_SCAN_MAX_ATTEMPTS; attempt++) {
        int triggered = scan_kbo_custom_events_once_for_date(today_yyyymmdd, source);
        if (triggered < 0) {
            return total_triggered > 0 ? total_triggered : -1;
        }
        if (triggered == 0) {
            return total_triggered;
        }
        total_triggered += triggered;
    }

    kbo_log_runtimef(
        "KBO custom event calendar scan stopped source=%s reason=attempt_limit total_triggered=%d",
        source != NULL ? source : "",
        total_triggered);
    return total_triggered;
}

/* GetTickCount64 is monotonic, so the last-tick comparison only needs an
 * age check; we still gate the log behind a CAS so concurrent callers do
 * not all win at once. */
static void kbo_custom_event_calendar_log_busy_throttled(uint32_t today_yyyymmdd, const char* source)
{
    ULONGLONG now = GetTickCount64();
    LONG64 last = InterlockedCompareExchange64(&g_kbo_custom_event_calendar_due_busy_log_ms, 0, 0);
    if (last > 0 && now - (ULONGLONG)last < KBO_CUSTOM_EVENT_BUSY_LOG_THROTTLE_MS) {
        return;
    }
    if (InterlockedCompareExchange64(
            &g_kbo_custom_event_calendar_due_busy_log_ms,
            (LONG64)now,
            last) != last) {
        return;
    }
    kbo_log_runtimef(
        "KBO custom event calendar due-through skipped source=%s today=%u reason=already_processing",
        source != NULL ? source : "",
        today_yyyymmdd);
}

static uint32_t kbo_custom_event_calendar_normalize_cursor(
    uint32_t previous_cursor,
    uint32_t today_yyyymmdd,
    const char* source)
{
    if (previous_cursor > today_yyyymmdd) {
        kbo_log_runtimef(
            "KBO custom event calendar cursor reset source=%s previous_cursor=%u today=%u reason=cursor_ahead_of_game_date",
            source != NULL ? source : "",
            previous_cursor,
            today_yyyymmdd);
        return 0u;
    }
    if (previous_cursor == today_yyyymmdd
            && kbo_custom_event_calendar_should_log_idle_due(0, 0, 0)) {
        kbo_log_runtimef(
            "KBO custom event calendar due-through repairing current cursor source=%s previous_cursor=%u today=%u reason=cursor_current",
            source != NULL ? source : "",
            previous_cursor,
            today_yyyymmdd);
    }
    return previous_cursor;
}

static void kbo_custom_event_calendar_run_all(
    uint32_t today_yyyymmdd,
    const char* source,
    KboCustomEventDueResults* out)
{
    out->foreign = kbo_schedule_foreign_priority_custom_events_for_date(source, today_yyyymmdd);
    out->asian = kbo_schedule_asian_games_custom_events_for_date(today_yyyymmdd, source);
    out->cbt = kbo_schedule_cbt_custom_events_for_date(today_yyyymmdd, source);
    out->independent = kbo_schedule_independent_team_acquisition_custom_events_for_date(today_yyyymmdd, source);
    out->scanned = kbo_custom_event_calendar_scan_until_idle(today_yyyymmdd, source);
    out->asian_hold = kbo_maintain_asian_games_restricted_players(today_yyyymmdd, source);
}

static int kbo_custom_event_due_any_critical_deferred(const KboCustomEventDueResults* r)
{
    return r->asian < 0 || r->cbt < 0 || r->independent < 0;
}

static int kbo_custom_event_due_any_changed(const KboCustomEventDueResults* r)
{
    return r->foreign > 0
        || r->asian > 0
        || r->asian_hold > 0
        || r->cbt > 0
        || r->independent > 0
        || r->scanned > 0;
}

static void kbo_custom_event_calendar_log_due_through(
    const char* source,
    uint32_t previous_cursor,
    uint32_t today_yyyymmdd,
    const KboCustomEventDueResults* r,
    int schedule_blocked,
    int deferred)
{
    kbo_log_runtimef(
        "KBO custom event calendar due-through source=%s previous_cursor=%u today=%u foreign=%d asian=%d asian_hold=%d cbt=%d independent=%d scanned=%d schedule_blocked=%d deferred=%d",
        source != NULL ? source : "",
        previous_cursor,
        today_yyyymmdd,
        r->foreign,
        r->asian,
        r->asian_hold,
        r->cbt,
        r->independent,
        r->scanned,
        schedule_blocked,
        deferred);
}

int kbo_process_custom_events_due_through(uint32_t today_yyyymmdd, const char* source)
{
    if (today_yyyymmdd == 0u) {
        return -1;
    }
    if (InterlockedCompareExchange(&g_kbo_custom_event_calendar_due_processing, 1, 0) != 0) {
        kbo_custom_event_calendar_log_busy_throttled(today_yyyymmdd, source);
        return -1;
    }

    uint32_t previous_cursor = kbo_custom_event_calendar_normalize_cursor(
        kbo_custom_event_calendar_read_cursor(),
        today_yyyymmdd,
        source);

    KboCustomEventDueResults r = {0};
    kbo_custom_event_calendar_run_all(today_yyyymmdd, source, &r);

    int schedule_blocked = kbo_custom_event_due_any_critical_deferred(&r);
    int deferred = schedule_blocked || r.scanned < 0;
    if (!deferred) {
        kbo_custom_event_calendar_write_cursor(today_yyyymmdd, source);
    }

    int changed = kbo_custom_event_due_any_changed(&r);
    if (kbo_custom_event_calendar_should_log_idle_due(changed, deferred, schedule_blocked)) {
        kbo_custom_event_calendar_log_due_through(
            source, previous_cursor, today_yyyymmdd, &r, schedule_blocked, deferred);
    }

    InterlockedExchange(&g_kbo_custom_event_calendar_due_processing, 0);
    if (deferred) {
        return -1;
    }
    return changed
        ? KBO_CUSTOM_EVENT_DUE_RESULT_CHANGED
        : KBO_CUSTOM_EVENT_DUE_RESULT_SCANNED_IDLE;
}
