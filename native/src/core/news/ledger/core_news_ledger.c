#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core_news_ledger.h"

#include <stdio.h>

#include "../../logging/core_log.h"
#include "../../sql/save_state/save_state_sqlite.h"
#include "sql/core_news_ledger_sql_store.h"

int kbo_custom_news_ledger_path(char* out, size_t out_size)
{
    return kbo_save_state_db_path(out, out_size);
}

static int kbo_custom_news_ledger_key(
    const char* domain,
    const char* key,
    char* out,
    size_t out_size)
{
    if (domain == NULL || domain[0] == '\0' || key == NULL || key[0] == '\0'
            || out == NULL || out_size == 0u) {
        return 0;
    }
    int len = snprintf(out, out_size, "%s|%s", domain, key);
    return len > 0 && len < (int)out_size;
}

int kbo_custom_news_ledger_completed(const char* domain, const char* key)
{
    char ledger_key[256] = {0};
    if (!kbo_custom_news_ledger_key(domain, key, ledger_key, sizeof(ledger_key))) {
        return 0;
    }

    return kbo_custom_news_ledger_sql_completed(ledger_key);
}

void kbo_custom_news_ledger_record(
    const char* domain,
    const char* key,
    const char* status,
    int result,
    const char* title,
    const char* detail,
    const char* source)
{
    char ledger_key[256] = {0};
    if (!kbo_custom_news_ledger_key(domain, key, ledger_key, sizeof(ledger_key))
            || status == NULL || status[0] == '\0') {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_custom_news_ledger_path(path, sizeof(path))) {
        kbo_log_runtimef(
            "KBO custom news ledger skipped source=%s domain=%s key=%s reason=path_unavailable",
            source != NULL ? source : "",
            domain != NULL ? domain : "",
            key != NULL ? key : "");
        return;
    }

    if (!kbo_custom_news_ledger_sql_record(
            ledger_key,
            domain,
            key,
            status,
            result,
            title,
            detail,
            source)) {
        kbo_log_runtimef(
            "KBO custom news ledger write failed source=%s domain=%s key=%s reason=sqlite_write_failed path=%s",
            source != NULL ? source : "",
            domain != NULL ? domain : "",
            key != NULL ? key : "",
            path);
    }
}

void kbo_custom_news_ledger_record_completed(
    const char* domain,
    const char* key,
    const char* detail,
    const char* source)
{
    kbo_custom_news_ledger_record(
        domain,
        key,
        "completed",
        1,
        NULL,
        detail,
        source);
}
