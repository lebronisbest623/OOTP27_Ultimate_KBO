#include "../common/custom_events_common.h"
#include "custom_event_dispatch.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/independent_acquisition/window/independent_acquisition_window.h"
#include "../../../foreign/intl_established_fa_postscan/api/intl_established_fa_postscan.h"
#include "../names/custom_event_names.h"

int kbo_custom_event_completed_state_is_valid(uint32_t league_id, uint32_t event_yyyymmdd, KboCustomEventKind kind)
{
    if (kind == KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE
            || kind == KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT) {
        return kbo_cbt_custom_event_completion_valid(league_id, event_yyyymmdd, kind);
    }
    if (kind == KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN) {
        return kbo_independent_team_acquisition_completion_valid(league_id, event_yyyymmdd);
    }
    return 1;
}

int kbo_dispatch_custom_event(
    uintptr_t event_ptr,
    const char* name,
    uint32_t event_yyyymmdd,
    uint32_t event_year,
    uint32_t event_month,
    uint32_t event_day,
    const char* source)
{
    return kbo_dispatch_custom_event_by_kind(
        event_ptr,
        kbo_custom_event_kind_from_name(name),
        event_yyyymmdd,
        event_year,
        event_month,
        event_day,
        source);
}

int kbo_dispatch_custom_event_by_kind(
    uintptr_t event_ptr,
    KboCustomEventKind kind,
    uint32_t event_yyyymmdd,
    uint32_t event_year,
    uint32_t event_month,
    uint32_t event_day,
    const char* source)
{
    if (kind == KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_OPEN) {
        if (g_kbo_foreign_priority_last_open_event_fired_date == event_yyyymmdd) {
            kbo_log_runtimef(
                "KBO custom event skipped duplicate by date source=%s kind=%s event_date=%04u-%02u-%02u",
                source != NULL ? source : "",
                kbo_custom_event_kind_key(kind),
                event_year,
                event_month,
                event_day);
            return -1;
        }
        uint32_t serial = kbo_date_serial(event_year, event_month, event_day);
        int opened = kbo_open_foreign_waiver_window(
            event_yyyymmdd,
            serial,
            "custom_event_monitor");
        if (opened) {
            g_kbo_foreign_priority_last_open_event_fired_date = event_yyyymmdd;
        }
        return opened;
    }

    if (kind == KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_CLOSE) {
        if (g_kbo_foreign_priority_last_close_event_fired_date == event_yyyymmdd) {
            kbo_log_runtimef(
                "KBO custom event skipped duplicate by date source=%s kind=%s event_date=%04u-%02u-%02u",
                source != NULL ? source : "",
                kbo_custom_event_kind_key(kind),
                event_year,
                event_month,
                event_day);
            return -1;
        }
        kbo_log_runtimef(
            "KBO custom event close marker reached source=%s date=%04u-%02u-%02u",
            source != NULL ? source : "",
            event_year,
            event_month,
            event_day);
        int announced = kbo_announce_foreign_waiver_results(event_yyyymmdd, source);
        if (!announced) {
            announced = 1;
        }
        if (announced) {
            g_kbo_foreign_priority_last_close_event_fired_date = event_yyyymmdd;
        }
        return announced;
    }

    if (kind == KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION) {
        char title[160] = {0};
        kbo_custom_event_title_for_kind(kind, title, sizeof(title));
        return run_kbo_custom_military_event(
            event_ptr,
            title[0] != '\0' ? title : kbo_custom_event_kind_key(kind),
            event_year,
            event_month,
            event_day,
            source);
    }

    if (kind == KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION) {
        return kbo_handle_asian_games_selection_event(event_yyyymmdd, source);
    }
    if (kind == KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE) {
        return kbo_handle_asian_games_departure_event(event_yyyymmdd, source);
    }
    if (kind == KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL) {
        return kbo_handle_asian_games_final_event(event_yyyymmdd, source);
    }
    if (kind == KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE) {
        return kbo_handle_cbt_deadline_event(event_yyyymmdd, source);
    }
    if (kind == KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT) {
        return kbo_handle_cbt_announcement_event(event_yyyymmdd, source);
    }
    if (kind == KBO_CUSTOM_EVENT_KIND_FA_DECLARATION) {
        return kbo_handle_fa_declaration_event(event_yyyymmdd, source);
    }
    if (kind == KBO_CUSTOM_EVENT_KIND_INTL_ESTABLISHED_FA) {
        return kbo_handle_intl_established_fa_event(event_yyyymmdd, source);
    }
    if (kind == KBO_CUSTOM_EVENT_KIND_INDEPENDENT_TEAM_ACQUISITION_OPEN) {
        return kbo_handle_independent_team_acquisition_open_event(event_yyyymmdd, source);
    }

    return 0;
}
