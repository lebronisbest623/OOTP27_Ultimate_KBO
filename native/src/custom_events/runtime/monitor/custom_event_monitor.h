#ifndef KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_MONITOR_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_MONITOR_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_custom_event_monitor_tick_for_date(
    uint32_t today_yyyymmdd,
    uint32_t* last_scheduled_yyyymmdd,
    uint32_t* last_scanned_yyyymmdd,
    uint32_t* last_fa_comp_yyyymmdd,
    const char* source);
DWORD WINAPI kbo_custom_event_monitor_thread(LPVOID parameter);
int start_kbo_custom_event_monitor(void);

#endif
