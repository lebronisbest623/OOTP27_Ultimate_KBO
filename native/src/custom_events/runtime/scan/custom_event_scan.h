#ifndef KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_SCAN_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_CUSTOM_EVENT_SCAN_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int scan_kbo_custom_events_once(const char* source);
int scan_kbo_custom_events_once_for_date(uint32_t current_yyyymmdd, const char* source);

#endif
