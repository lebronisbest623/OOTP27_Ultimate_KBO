#include "../secondary_draft_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../core/news/live/core_live_news.h"
#include "../../../core/sql/league_news/core_sql_league_news.h"

static void kbo_secondary_draft_append_pick_line(
    char* body,
    size_t body_size,
    const KboSecondaryDraftPickRow* pick)
{
    if (body == NULL || body_size == 0u || pick == NULL) {
        return;
    }
    size_t len = strlen(body);
    if (len + 1u >= body_size) {
        return;
    }
    snprintf(
        body + len,
        body_size - len,
        "\nRound %u pick %u: %s selected %s from %s.",
        pick->round,
        pick->pick_no,
        pick->to_team_name,
        pick->player_name,
        pick->from_team_name);
}

int kbo_secondary_draft_emit_window_news(
    uint32_t announcement_yyyymmdd,
    const KboSecondaryDraftWindow* window,
    const char* source)
{
    if (window == NULL || window->season == 0u || window->league_id == 0u || announcement_yyyymmdd == 0u) {
        return 0;
    }
    if (kbo_secondary_draft_sql_news_mark_exists(window->season, "window")) {
        return 0;
    }

    char title[160] = {0};
    snprintf(title, sizeof(title), "KBO Secondary Draft Window Opens");
    char body[768] = {0};
    snprintf(
        body,
        sizeof(body),
        "The KBO secondary draft process has opened for %u. Protection lists may be prepared through %04u-%02u-%02u. The draft is scheduled for %04u-%02u-%02u.",
        window->season,
        window->protection_deadline_yyyymmdd / 10000u,
        (window->protection_deadline_yyyymmdd / 100u) % 100u,
        window->protection_deadline_yyyymmdd % 100u,
        window->draft_yyyymmdd / 10000u,
        (window->draft_yyyymmdd / 100u) % 100u,
        window->draft_yyyymmdd % 100u);

    int sql_news = insert_kbo_league_news_table_sql(
        announcement_yyyymmdd / 10000u,
        (announcement_yyyymmdd / 100u) % 100u,
        announcement_yyyymmdd % 100u,
        window->league_id,
        title,
        body,
        source != NULL ? source : "secondary_draft_window_news");
    int live_news = create_kbo_native_live_news_with_body(
        announcement_yyyymmdd / 10000u,
        (announcement_yyyymmdd / 100u) % 100u,
        announcement_yyyymmdd % 100u,
        window->league_id,
        0u,
        title,
        body);
    if (sql_news || live_news) {
        kbo_secondary_draft_sql_mark_news(window->season, "window", source);
        return 1;
    }
    return 0;
}

int kbo_secondary_draft_emit_results_news(
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    const KboSecondaryDraftPickRow* picks,
    int pick_count,
    int candidate_count,
    int protected_count,
    int64_t cash_total,
    const char* source)
{
    uint32_t season = event_yyyymmdd / 10000u;
    if (season == 0u || league_id == 0u || event_yyyymmdd == 0u) {
        return 0;
    }
    if (kbo_secondary_draft_sql_news_mark_exists(season, "results")) {
        return 0;
    }

    char title[160] = {0};
    snprintf(title, sizeof(title), "KBO Secondary Draft Results");
    char body[2048] = {0};
    snprintf(
        body,
        sizeof(body),
        "The %u KBO secondary draft is complete. Picks: %d. Candidate pool: %d. Protected players: %d. Transfer fees: %lld KRW.",
        season,
        pick_count,
        candidate_count,
        protected_count,
        (long long)cash_total);
    int line_limit = pick_count < 12 ? pick_count : 12;
    for (int i = 0; i < line_limit; i++) {
        kbo_secondary_draft_append_pick_line(body, sizeof(body), &picks[i]);
    }

    int sql_news = insert_kbo_league_news_table_sql(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        title,
        body,
        source != NULL ? source : "secondary_draft_results_news");
    int live_news = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        0u,
        title,
        body);
    if (sql_news || live_news) {
        kbo_secondary_draft_sql_mark_news(season, "results", source);
        return 1;
    }
    return 0;
}
