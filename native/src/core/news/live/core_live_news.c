/* Core live news fanout helpers. */

#include <stdint.h>
#include <stdio.h>

#include "core_live_news.h"
#include "../../logging/core_log.h"
#include "../objects/core_news_object.h"
#include "../../sql/league_news/core_sql_league_news.h"
#include "../../dates/constants/kbo_date_constants.h"

int create_kbo_native_live_news_with_body(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body)
{
    if (title == NULL || title[0] == '\0') {
        return 0;
    }
    if (body == NULL || body[0] == '\0') {
        body = "";
    }

    int event_created = 0;
    int native_created = 0;
    int league_news_created = insert_kbo_league_news_table_sql(
        year,
        month,
        day,
        league_id,
        title,
        body,
        "native_live_news");
    kbo_log_runtimef(
        "native live news create_news_item skipped title=%s reason=unsafe_rva_call_disabled",
        title);

    int sql_created = insert_kbo_league_news_sql(
        year,
        month,
        day,
        league_id,
        message_type,
        title,
        body,
        "native_live_news");
    int real_created = create_kbo_real_add_news(
        year,
        month,
        day,
        league_id,
        message_type,
        title,
        body,
        "native_live_news");
    int core_created = 0;
    kbo_log_runtimef(
        "core message news skipped source=%s title=%s reason=unsafe_rva_call_disabled",
        "native_live_news",
        title);
    kbo_log_runtimef(
        "native live news result title=%s date=%04u-%02u-%02u league_id=%u type=%u event=%d league_news=%d sql=%d real=%d native=%d core=%d",
        title,
        year,
        month,
        day,
        league_id,
        message_type,
        event_created,
        league_news_created,
        sql_created,
        real_created,
        native_created,
        core_created);
    if (real_created) {
        return 2;
    }
    return event_created || league_news_created || sql_created || native_created || core_created;
}

int create_kbo_native_live_news_with_body_live_required(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body)
{
    if (title == NULL || title[0] == '\0'
            || league_id == 0u
            || year < KBO_HISTORY_YEAR_MIN || year > KBO_RECORD_YEAR_MAX
            || month < 1u || month > 12u
            || day < 1u || day > 31u) {
        return 0;
    }
    if (body == NULL || body[0] == '\0') {
        body = "";
    }

    int real_created = create_kbo_real_add_news(
        year,
        month,
        day,
        league_id,
        message_type,
        title,
        body,
        "native_live_news_live_required");
    if (!real_created) {
        kbo_log_runtimef(
            "native live news live-required deferred title=%s date=%04u-%02u-%02u league_id=%u type=%u reason=real_add_unavailable",
            title,
            year,
            month,
            day,
            league_id,
            message_type);
        return 0;
    }

    kbo_log_runtimef(
        "native live news live-required result title=%s date=%04u-%02u-%02u league_id=%u type=%u league_news=%d sql=%d real=%d persistence=ootp_real_add_only",
        title,
        year,
        month,
        day,
        league_id,
        message_type,
        0,
        0,
        real_created);
    return 2;
}

int create_kbo_native_live_news(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title)
{
    return create_kbo_native_live_news_with_body(
        year,
        month,
        day,
        league_id,
        message_type,
        title,
        NULL);
}
