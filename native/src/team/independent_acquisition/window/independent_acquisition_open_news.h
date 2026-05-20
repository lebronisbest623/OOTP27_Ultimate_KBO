#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_WINDOW_OPEN_NEWS_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_WINDOW_OPEN_NEWS_H_

#include <stdint.h>

int kbo_independent_team_acquisition_open_news_completed(
    uint32_t event_yyyymmdd,
    uint32_t league_id);
int kbo_emit_independent_team_acquisition_open_news(
    uint32_t event_yyyymmdd,
    const char* source);

#endif
