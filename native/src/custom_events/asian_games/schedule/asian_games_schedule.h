#ifndef KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_H_
#define KBOFIX_SRC_CUSTOM_EVENTS_ASIAN_GAMES_SCHEDULE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_schedule_asian_games_custom_events_for_date(uint32_t today, const char* source);
int kbo_schedule_asian_games_custom_events(const char* source);

#endif
