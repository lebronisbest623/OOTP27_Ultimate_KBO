#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "current_date_tick_capture.h"
#include "capture/current_date_tick_capture_internal.h"
#include "../core_current_date.h"
#include "../core_text_date.h"
#include "../../core_flags/api/flags_api.h"
#include "../../files/save_paths/core_save_paths.h"
#include "../../logging/core_log.h"

volatile LONG g_kbo_current_date_tick_event_write_cursor = 0;
volatile LONG g_kbo_current_date_tick_event_published_sequence = 0;
volatile LONG g_kbo_current_date_tick_last_published_date = 0;
uint32_t g_kbo_current_date_tick_event_dates[KBO_CURRENT_DATE_TICK_EVENT_RING_SIZE];
uint32_t g_kbo_current_date_tick_event_site_rvas[KBO_CURRENT_DATE_TICK_EVENT_RING_SIZE];
uint32_t g_kbo_current_date_tick_event_source_kinds[KBO_CURRENT_DATE_TICK_EVENT_RING_SIZE];
uint32_t g_kbo_current_date_tick_event_save_epochs[KBO_CURRENT_DATE_TICK_EVENT_RING_SIZE];

#define KBO_CURRENT_DATE_TICK_PUBLISH_LIVE 0
#define KBO_CURRENT_DATE_TICK_PUBLISH_SAVE_ENTER 1

static volatile LONG g_kbo_current_date_tick_rejected_log_count = 0;
static SRWLOCK g_kbo_current_date_tick_save_scope_lock = SRWLOCK_INIT;
static char g_kbo_current_date_tick_save_scope_path[MAX_PATH];

static uint32_t kbo_current_date_tick_source_kind(uint32_t site_rva, int publish_mode)
{
    if (publish_mode == KBO_CURRENT_DATE_TICK_PUBLISH_SAVE_ENTER) {
        return KBO_DATE_BOUNDARY_SOURCE_SAVE_ENTER;
    }
    if (site_rva == KBO_CURRENT_DATE_TICK_WATCHPOINT_SITE_RVA) {
        return KBO_DATE_BOUNDARY_SOURCE_WATCHPOINT;
    }
    return KBO_DATE_BOUNDARY_SOURCE_LIVE_POST_ADVANCE;
}

LONG kbo_current_date_tick_latest_sequence(void)
{
    return InterlockedCompareExchange(
        &g_kbo_current_date_tick_event_published_sequence,
        0,
        0);
}

static int kbo_current_date_tick_refresh_save_scope_for_source(
    uint32_t source_kind,
    uint32_t date,
    uint32_t site_rva)
{
    int save_scope_changed = kbo_date_boundary_refresh_save_scope(
        kbo_date_boundary_source_label(source_kind),
        NULL,
        NULL);
    if (save_scope_changed) {
        LONG previous_date = InterlockedExchange(
            &g_kbo_current_date_tick_last_published_date,
            0);
        kbo_current_date_tick_reset_sync_consumer_dates();
        if (previous_date != 0
                && kbo_current_date_tick_log_allowed((LONG*)&g_kbo_current_date_tick_rejected_log_count)) {
            kbo_log_runtimef(
                "KBO current date tick save epoch boundary reset previous=%u date=%u site=0x%x source=%s",
                (uint32_t)previous_date,
                date,
                site_rva,
                kbo_date_boundary_source_label(source_kind));
        }
    }
    return save_scope_changed;
}

static int kbo_current_date_tick_publish_core(
    uint32_t date,
    uint32_t site_rva,
    int publish_mode)
{
    uint32_t source_kind = kbo_current_date_tick_source_kind(site_rva, publish_mode);
    if (!kbo_yyyymmdd_valid(date)) {
        kbo_date_boundary_record_reject(
            date,
            0u,
            0u,
            site_rva,
            source_kind,
            0u);
        return 0;
    }

    int save_scope_changed = kbo_current_date_tick_refresh_save_scope_for_source(
        source_kind,
        date,
        site_rva);

    uint32_t accepted_previous_date = 0u;
    uint32_t accepted_expected_next = 0u;
    for (;;) {
        LONG previous_date = InterlockedCompareExchange(
            &g_kbo_current_date_tick_last_published_date,
            0,
            0);
        if ((uint32_t)previous_date == date) {
            if (publish_mode == KBO_CURRENT_DATE_TICK_PUBLISH_SAVE_ENTER) {
                accepted_previous_date = (uint32_t)previous_date;
                accepted_expected_next = kbo_yyyymmdd_add_days((uint32_t)previous_date, 1u);
                break;
            }
            return 0;
        }
        if (previous_date != 0) {
            if (publish_mode == KBO_CURRENT_DATE_TICK_PUBLISH_SAVE_ENTER) {
                kbo_date_boundary_record_reject(
                    date,
                    (uint32_t)previous_date,
                    0u,
                    site_rva,
                    source_kind,
                    KBO_DATE_BOUNDARY_FLAG_SAVE_ENTER);
                if (kbo_current_date_tick_log_allowed((LONG*)&g_kbo_current_date_tick_rejected_log_count)) {
                    kbo_log_runtimef(
                        "KBO current date tick publish rejected previous=%u date=%u site=0x%x reason=save_enter_not_initial_source",
                        (uint32_t)previous_date,
                        date,
                        site_rva);
                }
                return 0;
            }
            uint32_t expected_next = kbo_yyyymmdd_add_days((uint32_t)previous_date, 1u);
            if (expected_next == 0u || date != expected_next) {
                kbo_date_boundary_record_reject(
                    date,
                    (uint32_t)previous_date,
                    expected_next,
                    site_rva,
                    source_kind,
                    KBO_DATE_BOUNDARY_FLAG_NON_ADJACENT);
                if (kbo_current_date_tick_log_allowed((LONG*)&g_kbo_current_date_tick_rejected_log_count)) {
                    kbo_log_runtimef(
                        "KBO current date tick publish rejected previous=%u date=%u expected_next=%u site=0x%x reason=non_adjacent_live_date",
                        (uint32_t)previous_date,
                        date,
                        expected_next,
                        site_rva);
                }
                return 0;
            }
        }
        if (InterlockedCompareExchange(
                &g_kbo_current_date_tick_last_published_date,
                (LONG)date,
                previous_date) == previous_date) {
            accepted_previous_date = (uint32_t)previous_date;
            accepted_expected_next = previous_date != 0
                ? kbo_yyyymmdd_add_days((uint32_t)previous_date, 1u)
                : 0u;
            break;
        }
    }

    LONG event_no = InterlockedExchangeAdd(
        &g_kbo_current_date_tick_event_write_cursor,
        1);
    uint32_t index = (uint32_t)event_no & KBO_CURRENT_DATE_TICK_EVENT_RING_MASK;
    KboDateBoundaryContext boundary = {0};
    (void)kbo_date_boundary_record_accept(
        date,
        accepted_previous_date,
        accepted_expected_next,
        (uint32_t)event_no + 1u,
        site_rva,
        source_kind,
        save_scope_changed ? KBO_DATE_BOUNDARY_FLAG_SAVE_SCOPE_CHANGED : 0u,
        &boundary);
    g_kbo_current_date_tick_event_dates[index] = date;
    g_kbo_current_date_tick_event_site_rvas[index] = site_rva;
    g_kbo_current_date_tick_event_source_kinds[index] = source_kind;
    g_kbo_current_date_tick_event_save_epochs[index] = boundary.save_epoch;
    InterlockedIncrement(&g_kbo_current_date_tick_event_published_sequence);
    return 1;
}

int kbo_current_date_tick_publish(uint32_t date, uint32_t site_rva)
{
    return kbo_current_date_tick_publish_core(date, site_rva, KBO_CURRENT_DATE_TICK_PUBLISH_LIVE);
}

int kbo_current_date_tick_live_candidate_publishable(
    uint32_t date,
    uint32_t site_rva,
    uint32_t* out_previous_date,
    uint32_t* out_expected_next_date)
{
    if (out_previous_date != NULL) {
        *out_previous_date = 0u;
    }
    if (out_expected_next_date != NULL) {
        *out_expected_next_date = 0u;
    }
    if (!kbo_yyyymmdd_valid(date)) {
        return 0;
    }

    uint32_t source_kind = kbo_current_date_tick_source_kind(
        site_rva,
        KBO_CURRENT_DATE_TICK_PUBLISH_LIVE);
    (void)kbo_current_date_tick_refresh_save_scope_for_source(
        source_kind,
        date,
        site_rva);

    uint32_t previous_date = 0u;
    if (!kbo_current_date_tick_latest_published_date(&previous_date)
            || previous_date == 0u) {
        return 1;
    }
    if (out_previous_date != NULL) {
        *out_previous_date = previous_date;
    }
    if (date == previous_date) {
        return 1;
    }

    uint32_t expected_next = kbo_yyyymmdd_add_days(previous_date, 1u);
    if (out_expected_next_date != NULL) {
        *out_expected_next_date = expected_next;
    }
    return expected_next != 0u && date == expected_next;
}

void kbo_current_date_tick_force_resync(const char* label, const char* reason)
{
    LONG previous_date = InterlockedExchange(
        &g_kbo_current_date_tick_last_published_date,
        0);
    LONG previous_sequence = InterlockedExchange(
        &g_kbo_current_date_tick_event_published_sequence,
        0);
    LONG previous_write = InterlockedExchange(
        &g_kbo_current_date_tick_event_write_cursor,
        0);

    memset(
        g_kbo_current_date_tick_event_dates,
        0,
        sizeof(g_kbo_current_date_tick_event_dates));
    memset(
        g_kbo_current_date_tick_event_site_rvas,
        0,
        sizeof(g_kbo_current_date_tick_event_site_rvas));
    memset(
        g_kbo_current_date_tick_event_source_kinds,
        0,
        sizeof(g_kbo_current_date_tick_event_source_kinds));
    memset(
        g_kbo_current_date_tick_event_save_epochs,
        0,
        sizeof(g_kbo_current_date_tick_event_save_epochs));

    AcquireSRWLockExclusive(&g_kbo_current_date_tick_save_scope_lock);
    g_kbo_current_date_tick_save_scope_path[0] = '\0';
    ReleaseSRWLockExclusive(&g_kbo_current_date_tick_save_scope_lock);

    kbo_date_boundary_reset(label, reason);
    kbo_current_date_tick_reset_sync_consumer_dates();

    kbo_log_runtimef(
        "KBO current date tick force resync label=\"%s\" reason=%s previous_date=%u previous_sequence=%ld previous_write=%ld",
        kbo_current_date_tick_log_label(label),
        reason != NULL && reason[0] != '\0' ? reason : "unspecified",
        (uint32_t)previous_date,
        (long)previous_sequence,
        (long)previous_write);
}

int kbo_current_date_tick_publish_save_enter_current(
    const char* save_path,
    const char* label)
{
    if (save_path == NULL || save_path[0] == '\0') {
        return 0;
    }

    int save_scope_changed = 0;
    AcquireSRWLockExclusive(&g_kbo_current_date_tick_save_scope_lock);
    if (strcmp(g_kbo_current_date_tick_save_scope_path, save_path) != 0) {
        LONG previous_date = InterlockedExchange(&g_kbo_current_date_tick_last_published_date, 0);
        snprintf(g_kbo_current_date_tick_save_scope_path, sizeof(g_kbo_current_date_tick_save_scope_path), "%s", save_path);
        kbo_log_runtimef(
            "KBO current date tick save scope changed label=\"%s\" save=%s previous_date=%u",
            kbo_current_date_tick_log_label(label),
            save_path,
            (uint32_t)previous_date);
        save_scope_changed = 1;
    }
    ReleaseSRWLockExclusive(&g_kbo_current_date_tick_save_scope_lock);
    if (save_scope_changed) {
        kbo_current_date_tick_reset_sync_consumer_dates();
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today) || !kbo_yyyymmdd_valid(today)) {
        return 0;
    }

    int published = kbo_current_date_tick_publish_core(
        today,
        KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA,
        KBO_CURRENT_DATE_TICK_PUBLISH_SAVE_ENTER);
    if (published && kbo_current_date_tick_log_allowed(NULL)) {
        kbo_log_runtimef(
            "KBO current date tick save-enter published label=\"%s\" save=%s date=%u",
            kbo_current_date_tick_log_label(label),
            save_path,
            today);
    }
    if (published) {
        (void)kbo_current_date_tick_dispatch_sync_consumers(
            today,
            KBO_CURRENT_DATE_TICK_SAVE_ENTER_SITE_RVA);
    }
    return published;
}

const char* kbo_current_date_tick_log_label(const char* label)
{
    return (label != NULL && label[0] != '\0') ? label : "current_date_tick";
}

int kbo_current_date_tick_log_allowed(LONG* log_count)
{
    if (log_count == NULL) {
        return 1;
    }

    LONG index = InterlockedIncrement(log_count);
    return index <= 80 || (index % 500) == 0;
}
