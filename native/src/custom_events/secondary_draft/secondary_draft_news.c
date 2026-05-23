#include "secondary_draft_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/news/live/core_live_news.h"
#include "../../core/news/templates/core_news_templates.h"

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
        kbo_news_text_append(out, out_size, "No players were selected.");
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
            "- R%u #%u: %s, %s -> %s\n",
            picks[i].round,
            picks[i].pick_no,
            player_link,
            from_link,
            to_link);
    }
    if (pick_count > limit) {
        kbo_news_text_appendf(out, out_size, "- ... and %d more selections.\n", pick_count - limit);
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

    char season_text[16] = {0};
    char pick_count_text[16] = {0};
    char candidate_count_text[16] = {0};
    char protected_count_text[16] = {0};
    char total_cash_text[48] = {0};
    char summary_lines[2048] = {0};
    snprintf(season_text, sizeof(season_text), "%u", event_yyyymmdd / 10000u);
    snprintf(pick_count_text, sizeof(pick_count_text), "%d", pick_count);
    snprintf(candidate_count_text, sizeof(candidate_count_text), "%d", candidate_count);
    snprintf(protected_count_text, sizeof(protected_count_text), "%d", protected_count);
    kbo_secondary_draft_format_cash(cash_total, total_cash_text, sizeof(total_cash_text));
    kbo_secondary_draft_build_summary_lines(picks, pick_count, summary_lines, sizeof(summary_lines));

    const KboNewsTemplateVar vars[] = {
        {"season", season_text},
        {"pick_count", pick_count_text},
        {"candidate_count", candidate_count_text},
        {"protected_count", protected_count_text},
        {"total_cash", total_cash_text},
        {"summary_lines", summary_lines},
    };
    char title[180] = {0};
    char body[4096] = {0};
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

    return create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
}
