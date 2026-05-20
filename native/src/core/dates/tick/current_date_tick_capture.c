#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "current_date_tick_capture.h"
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

#define KBO_CURRENT_DATE_TICK_SYNC_CONSUMER_MAX 16u

typedef struct KboCurrentDateTickSyncConsumerSlot {
    char label[64];
    KboCurrentDateTickSyncConsumerFn callback;
    void* context;
    uint32_t phase;
    uint32_t ordinal;
    volatile LONG last_dispatched_date;
    volatile LONG processing_date;
    volatile LONG log_count;
} KboCurrentDateTickSyncConsumerSlot;

#define KBO_CURRENT_DATE_TICK_PUBLISH_LIVE 0
#define KBO_CURRENT_DATE_TICK_PUBLISH_SAVE_ENTER 1

static int kbo_current_date_tick_log_allowed(LONG* log_count);
static const char* kbo_current_date_tick_log_label(const char* label);
static volatile LONG g_kbo_current_date_tick_rejected_log_count = 0;
static SRWLOCK g_kbo_current_date_tick_save_scope_lock = SRWLOCK_INIT;
static char g_kbo_current_date_tick_save_scope_path[MAX_PATH];
static SRWLOCK g_kbo_current_date_tick_sync_consumer_lock = SRWLOCK_INIT;
static KboCurrentDateTickSyncConsumerSlot
    g_kbo_current_date_tick_sync_consumers[KBO_CURRENT_DATE_TICK_SYNC_CONSUMER_MAX];
static volatile LONG g_kbo_current_date_tick_sync_consumer_count = 0;
static volatile LONG g_kbo_current_date_tick_sync_consumer_next_ordinal = 0;

static void kbo_current_date_tick_reset_sync_consumer_dates(void);

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

static LONG kbo_current_date_tick_latest_sequence(void)
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

int kbo_current_date_tick_register_sync_consumer_ex(
    const char* label,
    uint32_t phase,
    KboCurrentDateTickSyncConsumerFn callback,
    void* context)
{
    if (callback == NULL) {
        return 0;
    }

    const char* safe_label = kbo_current_date_tick_log_label(label);
    AcquireSRWLockExclusive(&g_kbo_current_date_tick_sync_consumer_lock);
    LONG count = InterlockedCompareExchange(
        &g_kbo_current_date_tick_sync_consumer_count,
        0,
        0);
    for (LONG i = 0; i < count; ++i) {
        KboCurrentDateTickSyncConsumerSlot* slot =
            &g_kbo_current_date_tick_sync_consumers[i];
        if (slot->callback == callback
                && (label == NULL || strcmp(slot->label, safe_label) == 0)) {
            ReleaseSRWLockExclusive(&g_kbo_current_date_tick_sync_consumer_lock);
            return 1;
        }
    }
    if (count < 0 || (uint32_t)count >= KBO_CURRENT_DATE_TICK_SYNC_CONSUMER_MAX) {
        ReleaseSRWLockExclusive(&g_kbo_current_date_tick_sync_consumer_lock);
        kbo_log_runtimef(
            "KBO current date tick sync consumer register failed label=\"%s\" reason=capacity",
            safe_label);
        return 0;
    }

    KboCurrentDateTickSyncConsumerSlot* slot =
        &g_kbo_current_date_tick_sync_consumers[count];
    memset(slot, 0, sizeof(*slot));
    snprintf(slot->label, sizeof(slot->label), "%s", safe_label);
    slot->callback = callback;
    slot->context = context;
    slot->phase = phase;
    slot->ordinal = (uint32_t)InterlockedIncrement(&g_kbo_current_date_tick_sync_consumer_next_ordinal);
    InterlockedExchange(&g_kbo_current_date_tick_sync_consumer_count, count + 1);
    ReleaseSRWLockExclusive(&g_kbo_current_date_tick_sync_consumer_lock);

    kbo_log_runtimef(
        "KBO current date tick sync consumer registered label=\"%s\" phase=%u count=%ld",
        safe_label,
        phase,
        (long)(count + 1));
    return 1;
}

int kbo_current_date_tick_register_sync_consumer(
    const char* label,
    KboCurrentDateTickSyncConsumerFn callback,
    void* context)
{
    return kbo_current_date_tick_register_sync_consumer_ex(
        label,
        KBO_CURRENT_DATE_TICK_SYNC_PHASE_DOMAIN,
        callback,
        context);
}

static int kbo_current_date_tick_sync_consumer_before(LONG left, LONG right)
{
    KboCurrentDateTickSyncConsumerSlot* a =
        &g_kbo_current_date_tick_sync_consumers[left];
    KboCurrentDateTickSyncConsumerSlot* b =
        &g_kbo_current_date_tick_sync_consumers[right];
    if (a->phase != b->phase) {
        return a->phase < b->phase;
    }
    return a->ordinal < b->ordinal;
}

static void kbo_current_date_tick_sort_sync_consumer_indices(LONG* indices, LONG count)
{
    for (LONG i = 1; i < count; ++i) {
        LONG value = indices[i];
        LONG j = i;
        while (j > 0 && kbo_current_date_tick_sync_consumer_before(value, indices[j - 1])) {
            indices[j] = indices[j - 1];
            --j;
        }
        indices[j] = value;
    }
}

static int kbo_current_date_tick_dispatch_sync_consumers(uint32_t date, uint32_t site_rva)
{
    if (!kbo_yyyymmdd_valid(date)) {
        return 0;
    }

    LONG indices[KBO_CURRENT_DATE_TICK_SYNC_CONSUMER_MAX] = {0};
    LONG count = 0;
    AcquireSRWLockShared(&g_kbo_current_date_tick_sync_consumer_lock);
    LONG registered = InterlockedCompareExchange(
        &g_kbo_current_date_tick_sync_consumer_count,
        0,
        0);
    if (registered > (LONG)KBO_CURRENT_DATE_TICK_SYNC_CONSUMER_MAX) {
        registered = (LONG)KBO_CURRENT_DATE_TICK_SYNC_CONSUMER_MAX;
    }
    for (LONG i = 0; i < registered; ++i) {
        if (g_kbo_current_date_tick_sync_consumers[i].callback != NULL) {
            indices[count++] = i;
        }
    }
    ReleaseSRWLockShared(&g_kbo_current_date_tick_sync_consumer_lock);
    kbo_current_date_tick_sort_sync_consumer_indices(indices, count);

    int complete = 1;
    for (LONG i = 0; i < count; ++i) {
        KboCurrentDateTickSyncConsumerSlot* slot =
            &g_kbo_current_date_tick_sync_consumers[indices[i]];
        LONG last = InterlockedCompareExchange(&slot->last_dispatched_date, 0, 0);
        if ((uint32_t)last == date) {
            continue;
        }

        LONG processing = InterlockedCompareExchange(
            &slot->processing_date,
            (LONG)date,
            0);
        if (processing != 0) {
            complete = 0;
            continue;
        }

        int ok = 0;
        if ((uint32_t)InterlockedCompareExchange(&slot->last_dispatched_date, 0, 0) != date
                && slot->callback != NULL) {
            if (kbo_current_date_tick_log_allowed((LONG*)&slot->log_count)) {
                kbo_log_runtimef(
                    "KBO current date tick sync consumer event label=\"%s\" date=%u site=0x%x",
                    slot->label,
                    date,
                    site_rva);
            }
            ok = slot->callback(date, site_rva, slot->context);
        } else {
            ok = 1;
        }

        if (ok) {
            InterlockedExchange(&slot->last_dispatched_date, (LONG)date);
        } else {
            complete = 0;
            if (kbo_current_date_tick_log_allowed((LONG*)&slot->log_count)) {
                kbo_log_runtimef(
                    "KBO current date tick sync consumer deferred label=\"%s\" date=%u site=0x%x",
                    slot->label,
                    date,
                    site_rva);
            }
        }
        InterlockedExchange(&slot->processing_date, 0);
    }

    return complete;
}

int kbo_current_date_tick_publish_and_dispatch(uint32_t date, uint32_t site_rva)
{
    (void)kbo_current_date_tick_publish(date, site_rva);
    uint32_t latest = 0u;
    if (!kbo_current_date_tick_latest_published_date(&latest) || latest != date) {
        return 0;
    }
    return kbo_current_date_tick_dispatch_sync_consumers(date, site_rva);
}

static void kbo_current_date_tick_reset_sync_consumer_dates(void)
{
    AcquireSRWLockExclusive(&g_kbo_current_date_tick_sync_consumer_lock);
    LONG count = InterlockedCompareExchange(
        &g_kbo_current_date_tick_sync_consumer_count,
        0,
        0);
    if (count > (LONG)KBO_CURRENT_DATE_TICK_SYNC_CONSUMER_MAX) {
        count = (LONG)KBO_CURRENT_DATE_TICK_SYNC_CONSUMER_MAX;
    }
    for (LONG i = 0; i < count; ++i) {
        InterlockedExchange(
            &g_kbo_current_date_tick_sync_consumers[i].last_dispatched_date,
            0);
        InterlockedExchange(
            &g_kbo_current_date_tick_sync_consumers[i].processing_date,
            0);
    }
    ReleaseSRWLockExclusive(&g_kbo_current_date_tick_sync_consumer_lock);
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

int kbo_current_date_tick_latest_published_date(uint32_t* out_date)
{
    if (out_date != NULL) {
        *out_date = 0u;
    }

    uint32_t date = (uint32_t)InterlockedCompareExchange(
        &g_kbo_current_date_tick_last_published_date,
        0,
        0);
    if (!kbo_yyyymmdd_valid(date)) {
        return 0;
    }
    if (out_date != NULL) {
        *out_date = date;
    }
    return 1;
}

int kbo_current_date_tick_latest_boundary_context(KboDateBoundaryContext* out_context)
{
    return kbo_date_boundary_latest(out_context);
}

static int kbo_current_date_tick_publish_save_enter_current(
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

void kbo_current_date_tick_cursor_init(KboCurrentDateTickCursor* cursor)
{
    if (cursor == NULL) {
        return;
    }
    cursor->next_sequence = kbo_current_date_tick_latest_sequence();
}

void kbo_current_date_tick_cursor_skip_to_latest(KboCurrentDateTickCursor* cursor)
{
    if (cursor == NULL) {
        return;
    }
    cursor->next_sequence = kbo_current_date_tick_latest_sequence();
}

static const char* kbo_current_date_tick_log_label(const char* label)
{
    return (label != NULL && label[0] != '\0') ? label : "current_date_tick";
}

static int kbo_current_date_tick_log_allowed(LONG* log_count)
{
    if (log_count == NULL) {
        return 1;
    }

    LONG index = InterlockedIncrement(log_count);
    return index <= 80 || (index % 500) == 0;
}

static uint32_t kbo_current_date_tick_clamp_missed(LONG missed)
{
    if (missed <= 0) {
        return 0u;
    }
    if ((unsigned long)missed > UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)missed;
}

static void kbo_current_date_tick_log_overflow(
    const char* label,
    LONG* log_count,
    LONG next,
    LONG latest,
    LONG missed)
{
    if (!kbo_current_date_tick_log_allowed(log_count)) {
        return;
    }
    kbo_log_runtimef(
        "KBO current date tick ring overflow label=\"%s\" next=%ld latest=%ld missed=%ld",
        kbo_current_date_tick_log_label(label),
        (long)next,
        (long)latest,
        (long)missed);
}

static int kbo_current_date_tick_peek_ex(
    KboCurrentDateTickCursor* cursor,
    const char* label,
    LONG* overflow_log_count,
    KboCurrentDateTickEvent* out_event,
    uint32_t* out_missed_events)
{
    if (out_event != NULL) {
        *out_event = (KboCurrentDateTickEvent){0};
    }
    if (out_missed_events != NULL) {
        *out_missed_events = 0u;
    }
    if (cursor == NULL || out_event == NULL) {
        return 0;
    }

    LONG latest = kbo_current_date_tick_latest_sequence();
    if (cursor->next_sequence < 0 || cursor->next_sequence > latest) {
        cursor->next_sequence = latest;
        return 0;
    }
    if (cursor->next_sequence == latest) {
        return 0;
    }

    LONG available = latest - cursor->next_sequence;
    if (available > (LONG)KBO_CURRENT_DATE_TICK_EVENT_RING_SIZE) {
        LONG previous_next = cursor->next_sequence;
        LONG missed = available - (LONG)KBO_CURRENT_DATE_TICK_EVENT_RING_SIZE;
        cursor->next_sequence = latest - (LONG)KBO_CURRENT_DATE_TICK_EVENT_RING_SIZE;
        if (out_missed_events != NULL) {
            *out_missed_events = kbo_current_date_tick_clamp_missed(missed);
        }
        kbo_current_date_tick_log_overflow(
            label,
            overflow_log_count,
            previous_next,
            latest,
            missed);
    }

    LONG event_no = cursor->next_sequence;
    uint32_t index = (uint32_t)event_no & KBO_CURRENT_DATE_TICK_EVENT_RING_MASK;

    out_event->sequence = (uint32_t)event_no + 1u;
    out_event->date = g_kbo_current_date_tick_event_dates[index];
    out_event->site_rva = g_kbo_current_date_tick_event_site_rvas[index];
    out_event->source_kind = g_kbo_current_date_tick_event_source_kinds[index];
    out_event->save_epoch = g_kbo_current_date_tick_event_save_epochs[index];
    return 1;
}

int kbo_current_date_tick_next_ex(
    KboCurrentDateTickCursor* cursor,
    const char* label,
    KboCurrentDateTickEvent* out_event,
    uint32_t* out_missed_events)
{
    if (!kbo_current_date_tick_peek_ex(
            cursor,
            label,
            NULL,
            out_event,
            out_missed_events)) {
        return 0;
    }

    cursor->next_sequence++;
    return 1;
}

int kbo_current_date_tick_next(
    KboCurrentDateTickCursor* cursor,
    KboCurrentDateTickEvent* out_event)
{
    return kbo_current_date_tick_next_ex(cursor, "legacy", out_event, NULL);
}

static void kbo_current_date_tick_consumer_clear_pending(KboCurrentDateTickConsumer* consumer)
{
    if (consumer == NULL) {
        return;
    }
    consumer->pending_event = (KboCurrentDateTickEvent){0};
    consumer->pending_date = 0u;
    consumer->pending_valid = 0;
}

static int kbo_current_date_tick_consumer_pending_valid_value(uint32_t missed_events)
{
    if (missed_events >= (uint32_t)INT_MAX) {
        return INT_MAX;
    }
    return (int)missed_events + 1;
}

static uint32_t kbo_current_date_tick_consumer_pending_missed_events(
    const KboCurrentDateTickConsumer* consumer)
{
    if (consumer == NULL || consumer->pending_valid <= 1) {
        return 0u;
    }
    return (uint32_t)(consumer->pending_valid - 1);
}

static void kbo_current_date_tick_consumer_set_pending(
    KboCurrentDateTickConsumer* consumer,
    KboCurrentDateTickEvent event,
    uint32_t pending_date,
    uint32_t missed_events)
{
    consumer->pending_event = event;
    consumer->pending_date = pending_date;
    consumer->pending_valid = kbo_current_date_tick_consumer_pending_valid_value(missed_events);
}

static const char* kbo_current_date_tick_consumer_label(
    const KboCurrentDateTickConsumer* consumer)
{
    if (consumer == NULL || consumer->label[0] == '\0') {
        return "current_date_tick_consumer";
    }
    return consumer->label;
}

static void kbo_current_date_tick_consumer_reset_save(
    KboCurrentDateTickConsumer* consumer,
    const char* save_path,
    int preserve_pending_hooks)
{
    snprintf(consumer->save_path, sizeof(consumer->save_path), "%s", save_path);
    consumer->last_processed_date = 0u;
    kbo_current_date_tick_consumer_clear_pending(consumer);
    if (!preserve_pending_hooks) {
        kbo_current_date_tick_cursor_skip_to_latest(&consumer->cursor);
    }

    if (kbo_current_date_tick_log_allowed(&consumer->reset_log_count)) {
        kbo_log_runtimef(
            "KBO current date tick consumer save scope reset label=\"%s\" save=%s preserve_pending_hooks=%d",
            kbo_current_date_tick_consumer_label(consumer),
            consumer->save_path,
            preserve_pending_hooks);
    }
}

static int kbo_current_date_tick_consumer_refresh_save_path(
    KboCurrentDateTickConsumer* consumer,
    int* out_changed,
    int* out_first_known_save)
{
    if (out_changed != NULL) {
        *out_changed = 0;
    }
    if (out_first_known_save != NULL) {
        *out_first_known_save = 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))) {
        return 0;
    }

    if (consumer->save_path[0] != '\0' && strcmp(consumer->save_path, save_path) == 0) {
        return 1;
    }

    int first_known_save = consumer->save_path[0] == '\0';
    kbo_current_date_tick_consumer_reset_save(consumer, save_path, first_known_save);
    if (out_changed != NULL) {
        *out_changed = 1;
    }
    if (out_first_known_save != NULL) {
        *out_first_known_save = first_known_save;
    }
    return 1;
}

static int kbo_current_date_tick_consumer_publish_save_enter_current(
    KboCurrentDateTickConsumer* consumer)
{
    if ((consumer->flags & KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER) == 0u) {
        return 0;
    }
    return kbo_current_date_tick_publish_save_enter_current(
        consumer->save_path,
        kbo_current_date_tick_consumer_label(consumer));
}

static int kbo_current_date_tick_consumer_needs_save_enter_current(
    const KboCurrentDateTickConsumer* consumer)
{
    return consumer != NULL
        && (consumer->flags & KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER) != 0u
        && consumer->last_processed_date == 0u
        && !consumer->pending_valid;
}

static int kbo_current_date_tick_consumer_pending_work(
    const KboCurrentDateTickConsumer* consumer,
    KboCurrentDateTickWork* out_work)
{
    if (consumer == NULL || out_work == NULL || !consumer->pending_valid) {
        return 0;
    }

    *out_work = (KboCurrentDateTickWork){
        .date = consumer->pending_date,
        .event_date = consumer->pending_event.date,
        .site_rva = consumer->pending_event.site_rva,
        .sequence = consumer->pending_event.sequence,
        .missed_events = kbo_current_date_tick_consumer_pending_missed_events(consumer),
        .source_kind = consumer->pending_event.source_kind,
        .save_epoch = consumer->pending_event.save_epoch,
        .gap = 0
    };
    return 1;
}

void kbo_current_date_tick_consumer_init(
    KboCurrentDateTickConsumer* consumer,
    const char* label,
    uint32_t flags)
{
    if (consumer == NULL) {
        return;
    }

    memset(consumer, 0, sizeof(*consumer));
    kbo_current_date_tick_cursor_init(&consumer->cursor);
    snprintf(
        consumer->label,
        sizeof(consumer->label),
        "%s",
        label != NULL && label[0] != '\0' ? label : "current_date_tick_consumer");
    consumer->flags = flags;
}

int kbo_current_date_tick_consumer_next(
    KboCurrentDateTickConsumer* consumer,
    KboCurrentDateTickWork* out_work)
{
    if (out_work != NULL) {
        *out_work = (KboCurrentDateTickWork){0};
    }
    if (consumer == NULL || out_work == NULL) {
        return 0;
    }

    int save_changed = 0;
    int first_known_save = 0;
    if (!kbo_current_date_tick_consumer_refresh_save_path(
            consumer,
            &save_changed,
            &first_known_save)) {
        return 0;
    }
    if (save_changed || kbo_current_date_tick_consumer_needs_save_enter_current(consumer)) {
        kbo_current_date_tick_consumer_publish_save_enter_current(consumer);
    }

    if (consumer->pending_valid) {
        return kbo_current_date_tick_consumer_pending_work(consumer, out_work);
    }

    for (;;) {
        KboCurrentDateTickEvent event = {0};
        uint32_t missed_events = 0u;
        if (!kbo_current_date_tick_peek_ex(
                &consumer->cursor,
                kbo_current_date_tick_consumer_label(consumer),
                &consumer->overflow_log_count,
                &event,
                &missed_events)) {
            (void)first_known_save;
            return 0;
        }

        if (!kbo_yyyymmdd_valid(event.date)
                || (consumer->last_processed_date != 0u
                    && event.date <= consumer->last_processed_date)) {
            consumer->cursor.next_sequence++;
            continue;
        }

        kbo_current_date_tick_consumer_set_pending(
            consumer,
            event,
            event.date,
            missed_events);
        if (kbo_current_date_tick_log_allowed(&consumer->hook_log_count)) {
            kbo_log_runtimef(
                "KBO current date tick consumer hook event label=\"%s\" last=%u pending=%u event=%u site=0x%x seq=%u missed=%u",
                kbo_current_date_tick_consumer_label(consumer),
                consumer->last_processed_date,
                consumer->pending_date,
                event.date,
                event.site_rva,
                event.sequence,
                missed_events);
        }
        return kbo_current_date_tick_consumer_pending_work(consumer, out_work);
    }
}

void kbo_current_date_tick_consumer_mark_processed(KboCurrentDateTickConsumer* consumer)
{
    if (consumer == NULL || !consumer->pending_valid) {
        return;
    }

    uint32_t processed_date = consumer->pending_date;
    if (kbo_yyyymmdd_valid(processed_date)) {
        consumer->last_processed_date = processed_date;
    }

    if (consumer->pending_event.sequence != 0u
            && consumer->cursor.next_sequence < (LONG)consumer->pending_event.sequence) {
        consumer->cursor.next_sequence = (LONG)consumer->pending_event.sequence;
    }
    kbo_current_date_tick_consumer_clear_pending(consumer);
}

void kbo_current_date_tick_consumer_skip_to_latest(KboCurrentDateTickConsumer* consumer)
{
    if (consumer == NULL) {
        return;
    }

    kbo_current_date_tick_cursor_skip_to_latest(&consumer->cursor);
    kbo_current_date_tick_consumer_clear_pending(consumer);
}
