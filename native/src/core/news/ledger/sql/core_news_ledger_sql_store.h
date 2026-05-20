#ifndef KBOFIX_SRC_CORE_NEWS_LEDGER_SQL_CORE_NEWS_LEDGER_SQL_STORE_H_
#define KBOFIX_SRC_CORE_NEWS_LEDGER_SQL_CORE_NEWS_LEDGER_SQL_STORE_H_

int kbo_custom_news_ledger_sql_completed(const char* news_key);
int kbo_custom_news_ledger_sql_record(
    const char* news_key,
    const char* domain,
    const char* marker,
    const char* status,
    int result,
    const char* title,
    const char* detail,
    const char* source);

#endif
