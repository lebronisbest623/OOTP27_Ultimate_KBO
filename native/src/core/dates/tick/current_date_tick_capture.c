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

static int kbo_current_date_tick_log_allowed(LONG* log_count);

static LONG kbo_current_date_tick_latest_sequence(void)
{
    return InterlockedCompareExchange(
        &g_kbo_current_date_tick_event_published_sequence,
        0,
        0);
}

int kbo_current_date_tick_publish(uint32_t date, uint32_t site_rva)
{
    if (!kbo_yyyymmdd_valid(date)) {
        return 0;
    }
    for (;;) {
        LONG previous_date = InterlockedCompareExchange(
            &g_kbo_current_date_tick_last_published_date,
            0,
            0);
        if ((uint32_t)previous_date == date) {
            return 0;
        }
        if (InterlockedCompareExchange(
                &g_kbo_current_date_tick_last_published_date,
                (LONG)date,
                previous_date) == previous_date) {
            break;
        }
    }

    LONG event_no = InterlockedExchangeAdd(
        &g_kbo_current_date_tick_event_write_cursor,
        1);
    uint32_t index = (uint32_t)event_no & KBO_CURRENT_DATE_TICK_EVENT_RING_MASK;
    g_kbo_current_date_tick_event_dates[index] = date;
    g_kbo_current_date_tick_event_site_rvas[index] = site_rva;
    InterlockedIncrement(&g_kbo_current_date_tick_event_published_sequence);
    return 1;
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

static int kbo_current_date_tick_consumer_emit_save_enter_current(
    KboCurrentDateTickConsumer* consumer)
{
    if ((consumer->flags & KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER) == 0u) {
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today) || !kbo_yyyymmdd_valid(today)) {
        return 0;
    }

    KboCurrentDateTickEvent event = {
        .sequence = 0u,
        .date = today,
        .site_rva = 0u
    };
    kbo_current_date_tick_consumer_set_pending(consumer, event, today, 0u);
    return 1;
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
        .gap = consumer->pending_date != consumer->pending_event.date ? 1 : 0
    };
    return 1;
}

static uint32_t kbo_current_date_tick_consumer_first_pending_date(
    const KboCurrentDateTickConsumer* consumer,
    uint32_t event_date)
{
    if ((consumer->flags & KBO_CURRENT_DATE_TICK_CONSUMER_GAP_CATCHUP) == 0u
            || consumer->last_processed_date == 0u
            || event_date <= consumer->last_processed_date
            || !kbo_yyyymmdd_valid(consumer->last_processed_date)) {
        return event_date;
    }

    uint32_t next_date = kbo_yyyymmdd_add_days(consumer->last_processed_date, 1u);
    if (next_date != 0u && next_date <= event_date) {
        return next_date;
    }
    return event_date;
}

static int kbo_current_date_tick_consumer_can_observe_current(
    const KboCurrentDateTickConsumer* consumer)
{
    return consumer != NULL
        && (consumer->flags & KBO_CURRENT_DATE_TICK_CONSUMER_OBSERVE_CURRENT_WHEN_IDLE) != 0u
        && consumer->last_processed_date != 0u
        && !consumer->pending_valid;
}

static int kbo_current_date_tick_consumer_observe_current(
    KboCurrentDateTickConsumer* consumer,
    KboCurrentDateTickWork* out_work)
{
    if (!kbo_current_date_tick_consumer_can_observe_current(consumer)
            || out_work == NULL) {
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today) || !kbo_yyyymmdd_valid(today)) {
        return 0;
    }
    if (today <= consumer->last_processed_date) {
        return 0;
    }

    KboCurrentDateTickEvent event = {
        .sequence = 0u,
        .date = today,
        .site_rva = KBO_CURRENT_DATE_TICK_OBSERVED_CURRENT_SITE_RVA
    };
    kbo_current_date_tick_consumer_set_pending(
        consumer,
        event,
        kbo_current_date_tick_consumer_first_pending_date(consumer, today),
        0u);

    if (kbo_current_date_tick_log_allowed(&consumer->observed_log_count)) {
        kbo_log_runtimef(
            "KBO current date tick consumer observed current date label=\"%s\" last=%u current=%u",
            kbo_current_date_tick_consumer_label(consumer),
            consumer->last_processed_date,
            today);
    }
    return kbo_current_date_tick_consumer_pending_work(consumer, out_work);
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
    if ((!save_changed || !first_known_save)
            && (save_changed || kbo_current_date_tick_consumer_needs_save_enter_current(consumer))) {
        kbo_current_date_tick_consumer_emit_save_enter_current(consumer);
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
            if (first_known_save
                    && kbo_current_date_tick_consumer_needs_save_enter_current(consumer)) {
                kbo_current_date_tick_consumer_emit_save_enter_current(consumer);
                if (consumer->pending_valid) {
                    return kbo_current_date_tick_consumer_pending_work(consumer, out_work);
                }
            }
            return kbo_current_date_tick_consumer_observe_current(consumer, out_work);
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
            kbo_current_date_tick_consumer_first_pending_date(consumer, event.date),
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

    if (processed_date != consumer->pending_event.date
            && processed_date < consumer->pending_event.date) {
        uint32_t next_date = kbo_yyyymmdd_add_days(processed_date, 1u);
        if (next_date != 0u && next_date <= consumer->pending_event.date) {
            consumer->pending_date = next_date;
            return;
        }
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
