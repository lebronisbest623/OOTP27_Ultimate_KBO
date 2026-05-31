#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>


#include "../current_date_tick_consumer_internal.h"

#include "../../../../logging/core_log.h"
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

int kbo_current_date_tick_peek_ex(
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

