#include "../common/custom_events_common.h"
#include "custom_event_scan.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/dates/tick/current_date_tick_capture.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../allstar/allstar_league_context/allstar_league_context.h"
#include "../../../core/core_league_context_parts/event_manager/event_manager.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../../team/names/team_string.h"
#include "../ledger/custom_event_ledger.h"
#include "../runner/custom_event_runner.h"

static volatile LONG64 g_kbo_custom_event_scan_deferred_log_ms = 0;

static int kbo_custom_event_scan_should_log_deferred(void)
{
    ULONGLONG now = GetTickCount64();
    LONG64 last = InterlockedCompareExchange64(&g_kbo_custom_event_scan_deferred_log_ms, 0, 0);
    if (last > 0 && now >= (ULONGLONG)last && now - (ULONGLONG)last < 30000ull) {
        return 0;
    }
    return InterlockedCompareExchange64(&g_kbo_custom_event_scan_deferred_log_ms, (LONG64)now, last) == last;
}

int scan_kbo_custom_events_once_for_date(uint32_t current_yyyymmdd, const char* source)
{
    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0 || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return -1;
    }

    uint32_t current_year = current_yyyymmdd / 10000u;
    uint32_t current_month = (current_yyyymmdd / 100u) % 100u;
    uint32_t current_day = current_yyyymmdd % 100u;
    if (kbo_date_serial(current_year, current_month, current_day) == 0u) {
        return -1;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_count == 0) {
        return 0;
    }
    if (event_vector == 0 || event_count < 0 || event_count > 20000
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return -1;
    }

    int triggered = 0;
    int deferred = 0;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        uint32_t event_league_id = *(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET);
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
            continue;
        }

        char name[160] = {0};
        if (!copy_ootp_string_object_raw_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))) {
            continue;
        }
        if (!kbo_custom_event_name_matches_local(name)) {
            continue;
        }
        KboCustomEventKind kind = kbo_custom_event_kind_from_name(name);
        if (kind == KBO_CUSTOM_EVENT_KIND_UNKNOWN) {
            continue;
        }

        uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
        if (configured_league_id == 0u) {
            configured_league_id = kbo_resolve_kbo_league_id();
        }
        if (event_league_id != configured_league_id) {
            continue;
        }

        uint32_t event_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
        uint32_t event_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
        uint32_t event_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
        uint32_t event_over = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET);
        int due = 0;
        if (event_year < current_year) {
            due = 1;
        } else if (event_year == current_year) {
            if (event_month < current_month) {
                due = 1;
            } else if (event_month == current_month && event_day <= current_day) {
                due = 1;
            }
        }

        uint32_t event_yyyymmdd = event_year * 10000u + event_month * 100u + event_day;
        if (!due || event_over != 0 || kbo_custom_event_already_processed(event_ptr)) {
            continue;
        }
        int completed = kbo_custom_event_processed_marker_exists(event_yyyymmdd, name)
            || kbo_custom_event_processed_marker_exists_for_kind(event_yyyymmdd, kind)
            || kbo_custom_event_ledger_completed(event_league_id, event_yyyymmdd, kind);
        if (completed && kbo_custom_event_completed_state_is_valid(event_league_id, event_yyyymmdd, kind)) {
            kbo_mark_custom_event_processed(event_ptr);
            kbo_persist_custom_event_processed_marker(event_yyyymmdd, name, source);
            kbo_log_runtimef(
                "KBO custom event skipped completed source=%s kind=%s name=%s event_date=%04u-%02u-%02u",
                source != NULL ? source : "",
                kbo_custom_event_kind_key(kind),
                name,
                event_year,
                event_month,
                event_day);
            continue;
        }
        if (completed) {
            kbo_log_runtimef(
                "KBO custom event stale completion ignored source=%s kind=%s name=%s event_date=%04u-%02u-%02u",
                source != NULL ? source : "",
                kbo_custom_event_kind_key(kind),
                name,
                event_year,
                event_month,
                event_day);
        }

        int action_result = kbo_run_custom_event_by_kind(
            event_ptr,
            event_league_id,
            event_yyyymmdd,
            kind,
            name,
            source);
        if (action_result < 0) {
            continue;
        }
        if (action_result == 0) {
            deferred++;
        }

        if (action_result) {
            triggered++;
        }

        kbo_log_runtimef(
            "KBO custom event triggered source=%s name=%s event_date=%04u-%02u-%02u current=%04u-%02u-%02u over=%u event=%p action_result=%d",
            source != NULL ? source : "",
            name,
            event_year,
            event_month,
            event_day,
            current_year,
            current_month,
            current_day,
            event_over,
            (void*)event_ptr,
            action_result);

        if (triggered > 0) {
            break;
        }
    }

    if (triggered == 0 && deferred > 0 && kbo_custom_event_scan_should_log_deferred()) {
        kbo_log_runtimef(
            "KBO custom event scan deferred source=%s current=%04u-%02u-%02u deferred=%d count=%d manager=%p vector=%p",
            source != NULL ? source : "",
            current_year,
            current_month,
            current_day,
            deferred,
            event_count,
            (void*)event_manager,
            (void*)event_vector);
        return -1;
    }

    if (triggered > 0) {
        kbo_log_runtimef(
            "KBO custom event scan source=%s current=%04u-%02u-%02u triggered=%d count=%d manager=%p vector=%p",
            source != NULL ? source : "",
            current_year,
            current_month,
            current_day,
            triggered,
            event_count,
            (void*)event_manager,
            (void*)event_vector);
    }
    return triggered;
}

int scan_kbo_custom_events_once(const char* source)
{
    uint32_t current_yyyymmdd = 0u;
    if (!kbo_current_date_tick_latest_published_date(&current_yyyymmdd)) {
        return -1;
    }
    return scan_kbo_custom_events_once_for_date(current_yyyymmdd, source);
}
