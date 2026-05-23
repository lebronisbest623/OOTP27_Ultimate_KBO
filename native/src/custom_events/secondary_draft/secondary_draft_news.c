#include "secondary_draft_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/news/live/core_live_news.h"
#include "../../core/news/templates/core_news_templates.h"

#define KBO_SECONDARY_DRAFT_NEWS_KEY_WINDOW "window"
#define KBO_SECONDARY_DRAFT_NEWS_KEY_RESULTS "results"

static void kbo_secondary_draft_format_date(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (yyyymmdd == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    snprintf(
        out,
        out_size,
        "%04u-%02u-%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

static void kbo_secondary_draft_format_cash(int64_t amount, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    char raw[32] = {0};
    snprintf(raw, sizeof(raw), "%lld", (long long)amount);

    const char* digits = raw;
    int negative = 0;
    if (digits[0] == '-') {
        negative = 1;
        digits++;
    }
    size_t digit_count = strlen(digits);
    if (digit_count == 0u) {
        snprintf(out, out_size, "0");
        return;
    }
    size_t comma_count = (digit_count - 1u) / 3u;
    size_t needed = digit_count + comma_count + (negative ? 1u : 0u) + 1u;
    if (needed > out_size) {
        snprintf(out, out_size, "%lld", (long long)amount);
        return;
    }
    char* p = out;
    if (negative) {
        *p++ = '-';
    }
    size_t leading = digit_count % 3u;
    if (leading == 0u) {
        leading = 3u;
    }
    for (size_t i = 0u; i < digit_count; i++) {
        if (i != 0u && (i == leading || (i > leading && ((i - leading) % 3u) == 0u))) {
            *p++ = ',';
        }
        *p++ = digits[i];
    }
    *p = '\0';
}

int kbo_secondary_draft_emit_window_news(
    uint32_t announcement_yyyymmdd,
    const KboSecondaryDraftWindow* window,
    const char* source)
{
    if (announcement_yyyymmdd == 0u
            || window == NULL
            || window->season == 0u
            || window->league_id == 0u
            || kbo_secondary_draft_sql_news_mark_exists(window->season, KBO_SECONDARY_DRAFT_NEWS_KEY_WINDOW)) {
        return 0;
    }

    char season_text[16] = {0};
    char open_text[16] = {0};
    char deadline_text[16] = {0};
    char draft_text[16] = {0};
    char protected_limit_text[16] = {0};
    char round_count_text[16] = {0};
    char extra_rounds_text[16] = {0};
    char extra_teams_text[16] = {0};
    snprintf(season_text, sizeof(season_text), "%u", window->season);
    kbo_secondary_draft_format_date(window->protection_open_yyyymmdd, open_text, sizeof(open_text));
    kbo_secondary_draft_format_date(window->protection_deadline_yyyymmdd, deadline_text, sizeof(deadline_text));
    kbo_secondary_draft_format_date(window->draft_yyyymmdd, draft_text, sizeof(draft_text));
    snprintf(protected_limit_text, sizeof(protected_limit_text), "%d", KBO_SECONDARY_DRAFT_PROTECTED_COUNT);
    snprintf(round_count_text, sizeof(round_count_text), "%d", KBO_SECONDARY_DRAFT_ROUNDS);
    snprintf(extra_rounds_text, sizeof(extra_rounds_text), "%d", KBO_SECONDARY_DRAFT_ROUNDS - KBO_SECONDARY_DRAFT_BASE_ROUNDS);
    snprintf(extra_teams_text, sizeof(extra_teams_text), "%d", KBO_SECONDARY_DRAFT_EXTRA_TEAMS);

    const KboNewsTemplateVar vars[] = {
        {"season", season_text},
        {"protection_open_date", open_text},
        {"protection_deadline_date", deadline_text},
        {"draft_date", draft_text},
        {"protected_limit", protected_limit_text},
        {"round_count", round_count_text},
        {"extra_rounds", extra_rounds_text},
        {"extra_teams", extra_teams_text},
    };
    char title[180] = {0};
    char body[2048] = {0};
    int var_count = (int)(sizeof(vars) / sizeof(vars[0]));
    if (!kbo_news_template_render_key(
            "custom_event.secondary_draft.window.title",
            vars,
            var_count,
            title,
            sizeof(title),
            source)
            || !kbo_news_template_render_key(
                "custom_event.secondary_draft.window.body",
                vars,
                var_count,
                body,
                sizeof(body),
                source)) {
        snprintf(title, sizeof(title), "%s KBO Secondary Draft Window Opens", season_text);
        snprintf(
            body,
            sizeof(body),
            "The KBO secondary draft window is open from %s to %s. The draft is scheduled for %s.",
            open_text,
            deadline_text,
            draft_text);
    }

    int created = create_kbo_native_live_news_with_body(
        announcement_yyyymmdd / 10000u,
        (announcement_yyyymmdd / 100u) % 100u,
        announcement_yyyymmdd % 100u,
        window->league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    if (created) {
        kbo_secondary_draft_sql_mark_news(window->season, KBO_SECONDARY_DRAFT_NEWS_KEY_WINDOW, source);
    }
    return created;
}

static void kbo_secondary_draft_build_summary_lines(
    const KboSecondaryDraftPick* picks,
    int pick_count,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (pick_count <= 0 || picks == NULL) {
        kbo_news_text_append(out, out_size, "- none");
        return;
    }

    int limit = pick_count < KBO_SECONDARY_DRAFT_NEWS_PICK_LINES
        ? pick_count
        : KBO_SECONDARY_DRAFT_NEWS_PICK_LINES;
    for (int i = 0; i < limit; i++) {
        char player_link[160] = {0};
        char from_link[160] = {0};
        char to_link[160] = {0};
        snprintf(player_link, sizeof(player_link), "<%s:player#%u>", picks[i].player_name, picks[i].player_id);
        snprintf(from_link, sizeof(from_link), "<%s:team#%u>", picks[i].from_team_name, picks[i].from_team_id);
        snprintf(to_link, sizeof(to_link), "<%s:team#%u>", picks[i].to_team_name, picks[i].to_team_id);
        kbo_news_text_appendf(
            out,
            out_size,
            "- R%u #%u / %s / %s -> %s\n",
            picks[i].round,
            picks[i].pick_no,
            player_link,
            from_link,
            to_link);
    }
    if (pick_count > limit) {
        kbo_news_text_appendf(out, out_size, "- +%d more\n", pick_count - limit);
    }
}

int kbo_secondary_draft_emit_news(
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    const KboSecondaryDraftPick* picks,
    int pick_count,
    int candidate_count,
    int protected_count,
    int64_t cash_total,
    const char* source)
{
    if (event_yyyymmdd == 0u || league_id == 0u) {
        return 0;
    }
    uint32_t season = event_yyyymmdd / 10000u;
    if (kbo_secondary_draft_sql_news_mark_exists(season, KBO_SECONDARY_DRAFT_NEWS_KEY_RESULTS)) {
        return 0;
    }

    char season_text[16] = {0};
    char pick_count_text[16] = {0};
    char candidate_count_text[16] = {0};
    char protected_count_text[16] = {0};
    char total_cash_text[48] = {0};
    char summary_lines[2048] = {0};
    KboSecondaryDraftNewsContext context;
    memset(&context, 0, sizeof(context));
    snprintf(season_text, sizeof(season_text), "%u", season);
    snprintf(pick_count_text, sizeof(pick_count_text), "%d", pick_count);
    snprintf(candidate_count_text, sizeof(candidate_count_text), "%d", candidate_count);
    snprintf(protected_count_text, sizeof(protected_count_text), "%d", protected_count);
    kbo_secondary_draft_format_cash(cash_total, total_cash_text, sizeof(total_cash_text));
    kbo_secondary_draft_build_summary_lines(picks, pick_count, summary_lines, sizeof(summary_lines));
    kbo_secondary_draft_build_news_context(picks, pick_count, &context);

    const KboNewsTemplateVar vars[] = {
        {"season", season_text},
        {"pick_count", pick_count_text},
        {"candidate_count", candidate_count_text},
        {"protected_count", protected_count_text},
        {"total_cash", total_cash_text},
        {"lead_pick_line", context.lead_pick_line},
        {"round_summary_lines", context.round_summary_lines},
        {"incoming_team_lines", context.incoming_team_lines},
        {"source_team_lines", context.source_team_lines},
        {"summary_lines", summary_lines},
    };
    char title[180] = {0};
    char body[8192] = {0};
    int var_count = (int)(sizeof(vars) / sizeof(vars[0]));
    if (!kbo_news_template_render_key(
            "custom_event.secondary_draft.results.title",
            vars,
            var_count,
            title,
            sizeof(title),
            source)
            || !kbo_news_template_render_key(
                "custom_event.secondary_draft.results.body",
                vars,
                var_count,
                body,
                sizeof(body),
                source)) {
        snprintf(title, sizeof(title), "%s KBO Secondary Draft Results", season_text);
        snprintf(
            body,
            sizeof(body),
            "The KBO secondary draft has been completed with %s selections.\n\n%s",
            pick_count_text,
            summary_lines);
    }

    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    if (created) {
        kbo_secondary_draft_sql_mark_news(season, KBO_SECONDARY_DRAFT_NEWS_KEY_RESULTS, source);
    }
    return created;
}

int kbo_secondary_draft_emit_stored_results_news(
    uint32_t season,
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    int candidate_count,
    int protected_count,
    int64_t cash_total,
    const char* source)
{
    if (season == 0u || event_yyyymmdd == 0u || league_id == 0u) {
        return 0;
    }
    KboSecondaryDraftResultRow rows[KBO_SECONDARY_DRAFT_TEAM_MAX * KBO_SECONDARY_DRAFT_ROUNDS];
    memset(rows, 0, sizeof(rows));
    int row_count = kbo_secondary_draft_load_result_rows(
        season,
        rows,
        (int)(sizeof(rows) / sizeof(rows[0])));
    if (row_count <= 0) {
        return 0;
    }

    KboSecondaryDraftPick picks[KBO_SECONDARY_DRAFT_TEAM_MAX * KBO_SECONDARY_DRAFT_ROUNDS];
    memset(picks, 0, sizeof(picks));
    for (int i = 0; i < row_count; i++) {
        picks[i].round = rows[i].round;
        picks[i].pick_no = rows[i].pick_no;
        picks[i].player_id = rows[i].player_id;
        picks[i].from_team_id = rows[i].from_team_id;
        picks[i].to_team_id = rows[i].to_team_id;
        picks[i].cash_amount = rows[i].cash_amount;
        picks[i].cash_applied = rows[i].cash_applied;
        picks[i].moved = rows[i].moved;
        snprintf(picks[i].player_name, sizeof(picks[i].player_name), "%s", rows[i].player_name);
        snprintf(picks[i].from_team_name, sizeof(picks[i].from_team_name), "%s", rows[i].from_team_name);
        snprintf(picks[i].to_team_name, sizeof(picks[i].to_team_name), "%s", rows[i].to_team_name);
    }
    return kbo_secondary_draft_emit_news(
        event_yyyymmdd,
        league_id,
        picks,
        row_count,
        candidate_count,
        protected_count,
        cash_total,
        source);
}
