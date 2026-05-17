#include "custom_event_runner.h"

#include <stdio.h>

#include "../../../core/logging/core_log.h"
#include "../dispatch/custom_event_dispatch.h"
#include "../ledger/custom_event_ledger.h"
#include "../markers/custom_event_markers.h"

static volatile LONG64 g_kbo_custom_event_running_keys[KBO_CUSTOM_EVENT_KIND_COUNT] = {0};

static LONG64 kbo_custom_event_running_key(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind)
{
    if (event_yyyymmdd == 0u
            || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN
            || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
        return 0;
    }

    uint64_t key = ((uint64_t)(uint32_t)kind << 56)
        | ((uint64_t)(league_id & 0x00ffffffu) << 32)
        | (uint64_t)event_yyyymmdd;
    if (key == 0ull) {
        key = 1ull;
    }
    return (LONG64)key;
}

static int kbo_custom_event_try_enter_run(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* source)
{
    LONG64 key = kbo_custom_event_running_key(league_id, event_yyyymmdd, kind);
    if (key == 0) {
        return 0;
    }

    LONG64 existing = InterlockedCompareExchange64(
        &g_kbo_custom_event_running_keys[kind],
        key,
        0);
    if (existing == 0) {
        return 1;
    }

    kbo_log_runtimef(
        "KBO custom event runner skipped in-progress source=%s kind=%s date=%u league_id=%u",
        source != NULL ? source : "",
        kbo_custom_event_kind_key(kind),
        event_yyyymmdd,
        league_id);
    return 0;
}

static void kbo_custom_event_leave_run(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind)
{
    LONG64 key = kbo_custom_event_running_key(league_id, event_yyyymmdd, kind);
    if (key == 0) {
        return;
    }
    InterlockedCompareExchange64(&g_kbo_custom_event_running_keys[kind], 0, key);
}

int kbo_run_custom_event_by_kind(
    uintptr_t event_ptr,
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    KboCustomEventKind kind,
    const char* title,
    const char* source)
{
    if (event_yyyymmdd == 0u
            || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN
            || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
        return -1;
    }

    if (!kbo_custom_event_try_enter_run(league_id, event_yyyymmdd, kind, source)) {
        return KBO_CUSTOM_EVENT_RUN_IN_PROGRESS;
    }

    int completed = kbo_custom_event_ledger_completed(league_id, event_yyyymmdd, kind)
        || kbo_custom_event_processed_marker_exists_for_kind(event_yyyymmdd, kind);
    if (completed && kbo_custom_event_completed_state_is_valid(league_id, event_yyyymmdd, kind)) {
        if (event_ptr != 0) {
            kbo_mark_custom_event_processed(event_ptr);
        }
        if (title != NULL && title[0] != '\0') {
            kbo_persist_custom_event_processed_marker(event_yyyymmdd, title, source);
        }
        kbo_log_runtimef(
            "KBO custom event runner skipped completed source=%s kind=%s date=%u league_id=%u",
            source != NULL ? source : "",
            kbo_custom_event_kind_key(kind),
            event_yyyymmdd,
            league_id);
        kbo_custom_event_leave_run(league_id, event_yyyymmdd, kind);
        return KBO_CUSTOM_EVENT_RUN_ALREADY_COMPLETED;
    }
    if (completed) {
        kbo_log_runtimef(
            "KBO custom event runner stale completion ignored source=%s kind=%s date=%u league_id=%u",
            source != NULL ? source : "",
            kbo_custom_event_kind_key(kind),
            event_yyyymmdd,
            league_id);
    }

    uint32_t event_year = event_yyyymmdd / 10000u;
    uint32_t event_month = (event_yyyymmdd / 100u) % 100u;
    uint32_t event_day = event_yyyymmdd % 100u;
    int result = kbo_dispatch_custom_event_by_kind(
        event_ptr,
        kind,
        event_yyyymmdd,
        event_year,
        event_month,
        event_day,
        source);

    if (result > 0) {
        if (event_ptr != 0) {
            kbo_mark_custom_event_processed(event_ptr);
        }
        if (title != NULL && title[0] != '\0') {
            kbo_persist_custom_event_processed_marker(event_yyyymmdd, title, source);
        }
        kbo_custom_event_ledger_record(
            league_id,
            event_yyyymmdd,
            kind,
            "completed",
            result,
            title,
            "handler_completed",
            source);
        kbo_custom_event_leave_run(league_id, event_yyyymmdd, kind);
        return result;
    }

    if (result == 0) {
        kbo_custom_event_ledger_record(
            league_id,
            event_yyyymmdd,
            kind,
            "deferred",
            result,
            title,
            "handler_deferred",
            source);
        kbo_custom_event_leave_run(league_id, event_yyyymmdd, kind);
        return 0;
    }

    kbo_custom_event_ledger_record(
        league_id,
        event_yyyymmdd,
        kind,
        "failed",
        result,
        title,
        "handler_failed",
        source);
    kbo_custom_event_leave_run(league_id, event_yyyymmdd, kind);
    return -1;
}
