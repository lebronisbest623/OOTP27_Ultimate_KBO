#include "../internal/cbt_internal.h"

#include "../../core/news/ledger/core_news_ledger.h"

#define KBO_CBT_NEWS_LEDGER_DOMAIN "cbt"

int kbo_cbt_news_marker_exists(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return 0;
    }
    return kbo_custom_news_ledger_completed(KBO_CBT_NEWS_LEDGER_DOMAIN, key);
}

void kbo_cbt_news_persist_marker(const char* key, const char* source)
{
    if (key == NULL || key[0] == '\0' || kbo_cbt_news_marker_exists(key)) {
        return;
    }
    kbo_custom_news_ledger_record_completed(
        KBO_CBT_NEWS_LEDGER_DOMAIN,
        key,
        "news_marker_persist",
        source);
}
