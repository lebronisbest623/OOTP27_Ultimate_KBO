#ifndef KBOFIX_SRC_CORE_DATES_TICK_CURRENT_DATE_TICK_CAPTURE_H_
#define KBOFIX_SRC_CORE_DATES_TICK_CURRENT_DATE_TICK_CAPTURE_H_

#include <stdint.h>
#include <windows.h>

#define KBO_CURRENT_DATE_TICK_EVENT_RING_SIZE 256u
#define KBO_CURRENT_DATE_TICK_EVENT_RING_MASK (KBO_CURRENT_DATE_TICK_EVENT_RING_SIZE - 1u)

typedef struct KboCurrentDateTickEvent {
    uint32_t sequence;
    uint32_t date;
    uint32_t site_rva;
} KboCurrentDateTickEvent;

typedef struct KboCurrentDateTickCursor {
    LONG next_sequence;
} KboCurrentDateTickCursor;

typedef struct KboCurrentDateTickWork {
    uint32_t date;
    uint32_t event_date;
    uint32_t site_rva;
    uint32_t sequence;
    uint32_t missed_events;
    int gap;
} KboCurrentDateTickWork;

typedef struct KboCurrentDateTickConsumer {
    KboCurrentDateTickCursor cursor;
    KboCurrentDateTickEvent pending_event;
    char save_path[MAX_PATH];
    char label[64];
    uint32_t last_processed_date;
    uint32_t pending_date;
    uint32_t flags;
    int pending_valid;
    LONG overflow_log_count;
    LONG reset_log_count;
    LONG observed_log_count;
    LONG hook_log_count;
} KboCurrentDateTickConsumer;

#define KBO_CURRENT_DATE_TICK_CONSUMER_EMIT_CURRENT_ON_SAVE_ENTER 1u
#define KBO_CURRENT_DATE_TICK_CONSUMER_GAP_CATCHUP 2u
#define KBO_CURRENT_DATE_TICK_CONSUMER_OBSERVE_CURRENT_WHEN_IDLE 4u
#define KBO_CURRENT_DATE_TICK_OBSERVED_CURRENT_SITE_RVA 0xffffffffu

extern volatile LONG g_kbo_current_date_tick_event_write_cursor;
extern volatile LONG g_kbo_current_date_tick_event_published_sequence;
extern volatile LONG g_kbo_current_date_tick_last_published_date;
extern uint32_t g_kbo_current_date_tick_event_dates[KBO_CURRENT_DATE_TICK_EVENT_RING_SIZE];
extern uint32_t g_kbo_current_date_tick_event_site_rvas[KBO_CURRENT_DATE_TICK_EVENT_RING_SIZE];

int kbo_current_date_tick_publish(uint32_t date, uint32_t site_rva);
void kbo_current_date_tick_cursor_init(KboCurrentDateTickCursor* cursor);
void kbo_current_date_tick_cursor_skip_to_latest(KboCurrentDateTickCursor* cursor);
int kbo_current_date_tick_next_ex(
    KboCurrentDateTickCursor* cursor,
    const char* label,
    KboCurrentDateTickEvent* out_event,
    uint32_t* out_missed_events);
int kbo_current_date_tick_next(
    KboCurrentDateTickCursor* cursor,
    KboCurrentDateTickEvent* out_event);
void kbo_current_date_tick_consumer_init(
    KboCurrentDateTickConsumer* consumer,
    const char* label,
    uint32_t flags);
int kbo_current_date_tick_consumer_next(
    KboCurrentDateTickConsumer* consumer,
    KboCurrentDateTickWork* out_work);
void kbo_current_date_tick_consumer_mark_processed(KboCurrentDateTickConsumer* consumer);
void kbo_current_date_tick_consumer_skip_to_latest(KboCurrentDateTickConsumer* consumer);

#endif
