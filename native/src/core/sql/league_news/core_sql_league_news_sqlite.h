#ifndef KBO_CORE_SQL_LEAGUE_NEWS_SQLITE_H
#define KBO_CORE_SQL_LEAGUE_NEWS_SQLITE_H

#include <stdint.h>

int kbo_insert_league_news_table_text_data_fallback(
    const char* create_sql,
    const char* delete_sql,
    const char* insert_sql,
    const char* source,
    const char* title,
    const char* news_date,
    uint32_t league_id);

#endif
