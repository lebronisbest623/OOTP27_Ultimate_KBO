#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_open_news.h"

#include <stdio.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/news/ledger/core_news_ledger.h"
#include "../../../core/news/objects/core_news_object.h"
#include "../../../core/news/templates/core_news_templates.h"
#include "../../../core/sql/league_news/core_sql_league_news.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"

#define KBO_INDEPENDENT_ACQUISITION_NEWS_LEDGER_DOMAIN "independent_acquisition"

static uint32_t kbo_independent_team_acquisition_event_league_id(void)
{
    uint32_t league_id = kbo_resolve_kbo_league_id();
    return league_id;
}

static int kbo_independent_team_acquisition_open_news_marker(
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    char* out,
    size_t out_size)
{
    if (event_yyyymmdd == 0u || league_id == 0u || out == NULL || out_size == 0u) {
        return 0;
    }
    int len = snprintf(out, out_size, "open|%u|%u", event_yyyymmdd, league_id);
    return len > 0 && len < (int)out_size;
}

int kbo_independent_team_acquisition_open_news_completed(
    uint32_t event_yyyymmdd,
    uint32_t league_id)
{
    char marker[64] = {0};
    if (!kbo_independent_team_acquisition_open_news_marker(
            event_yyyymmdd,
            league_id,
            marker,
            sizeof(marker))) {
        return 0;
    }
    return kbo_custom_news_ledger_completed(
        KBO_INDEPENDENT_ACQUISITION_NEWS_LEDGER_DOMAIN,
        marker);
}

static int kbo_independent_team_acquisition_build_news_text(
    char* title,
    size_t title_size,
    char* body,
    size_t body_size,
    const char* source)
{
    if (title == NULL || title_size == 0u || body == NULL || body_size == 0u) {
        return 0;
    }

    title[0] = '\0';
    body[0] = '\0';
    if (!kbo_news_template_render_key(
            "independent_acquisition.open.title",
            NULL,
            0,
            title,
            title_size,
            source != NULL ? source : "independent_team_acquisition")) {
        snprintf(title, title_size, "Futures Independent Club Signing Window Opens");
    }
    if (!kbo_news_template_render_key(
            "independent_acquisition.open.news.body",
            NULL,
            0,
            body,
            body_size,
            source != NULL ? source : "independent_team_acquisition")) {
        snprintf(
            body,
            body_size,
            "KBO opened the signing window for independent clubs playing in the Futures League.\n\n"
            "The window is scheduled from the opening day of the independent club's league, "
            "and player acquisitions will be handled under the separate independent club process.");
    }

    return title[0] != '\0' && body[0] != '\0';
}

int kbo_emit_independent_team_acquisition_open_news(
    uint32_t event_yyyymmdd,
    const char* source)
{
    uint32_t league_id = kbo_independent_team_acquisition_event_league_id();
    if (event_yyyymmdd == 0u || league_id == 0u) {
        kbo_log_runtimef(
            "KBO independent futures acquisition news skipped source=%s date=%u league_id=%u reason=invalid_context",
            source != NULL ? source : "",
            event_yyyymmdd,
            league_id);
        return 0;
    }

    char marker[64] = {0};
    if (!kbo_independent_team_acquisition_open_news_marker(
            event_yyyymmdd,
            league_id,
            marker,
            sizeof(marker))) {
        kbo_log_runtimef(
            "KBO independent futures acquisition news skipped source=%s date=%u league_id=%u reason=marker_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd,
            league_id);
        return 0;
    }
    if (kbo_custom_news_ledger_completed(
            KBO_INDEPENDENT_ACQUISITION_NEWS_LEDGER_DOMAIN,
            marker)) {
        kbo_log_runtimef(
            "KBO independent futures acquisition news skipped source=%s date=%u league_id=%u reason=marker_exists",
            source != NULL ? source : "",
            event_yyyymmdd,
            league_id);
        return 1;
    }

    char title[180] = {0};
    char body[2048] = {0};
    if (!kbo_independent_team_acquisition_build_news_text(
            title,
            sizeof(title),
            body,
            sizeof(body),
            source)) {
        kbo_log_runtimef(
            "KBO independent futures acquisition news skipped source=%s date=%u league_id=%u reason=text_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd,
            league_id);
        return 0;
    }

    int real_created = create_kbo_real_add_news(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body,
        "independent_acquisition_open");
    int league_news_created = insert_kbo_league_news_table_sql(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        title,
        body,
        "independent_acquisition_open");
    int message_created = insert_kbo_league_news_sql(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body,
        "independent_acquisition_open");
    int created = real_created != 0 || league_news_created != 0 || message_created != 0;
    if (created) {
        kbo_custom_news_ledger_record_completed(
            KBO_INDEPENDENT_ACQUISITION_NEWS_LEDGER_DOMAIN,
            marker,
            "open_news_created",
            source);
    }
    kbo_log_runtimef(
        "KBO independent futures acquisition news source=%s title=%s date=%u league_id=%u real=%d league_news=%d messages=%d created=%d",
        source != NULL ? source : "",
        title,
        event_yyyymmdd,
        league_id,
        real_created,
        league_news_created,
        message_created,
        created);
    return created;
}
