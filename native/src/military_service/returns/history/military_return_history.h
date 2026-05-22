#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_RETURN_HISTORY_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_RETURN_HISTORY_H_

#include <stdint.h>

int kbo_military_return_history_was_recorded(uint32_t player_id, uint32_t history_yyyymmdd);
int kbo_mark_military_return_history_once(uint32_t player_id, uint32_t history_yyyymmdd);

#endif
