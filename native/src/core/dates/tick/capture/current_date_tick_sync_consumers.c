#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "current_date_tick_capture_internal.h"

#include <stdio.h>
#include <string.h>

#include "../../core_text_date.h"
#include "../../../logging/core_log.h"

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

static SRWLOCK g_kbo_current_date_tick_sync_consumer_lock = SRWLOCK_INIT;
static KboCurrentDateTickSyncConsumerSlot
    g_kbo_current_date_tick_sync_consumers[KBO_CURRENT_DATE_TICK_SYNC_CONSUMER_MAX];
static volatile LONG g_kbo_current_date_tick_sync_consumer_count = 0;
static volatile LONG g_kbo_current_date_tick_sync_consumer_next_ordinal = 0;

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
    slot->ordinal = (uint32_t)InterlockedIncrement(
        &g_kbo_current_date_tick_sync_consumer_next_ordinal);
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

int kbo_current_date_tick_dispatch_sync_consumers(uint32_t date, uint32_t site_rva)
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

void kbo_current_date_tick_reset_sync_consumer_dates(void)
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
