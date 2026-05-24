#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../../core/news/live/core_live_news.h"
#include "../../../core/news/templates/core_news_templates.h"
#include "../../../core/logging/core_log.h"
#include "../fa_compensation_news_transfer.h"
#include "../fa_compensation_news_transfer_internal.h"
#include "../../../core/dates/constants/kbo_date_constants.h"

void kbo_emit_fa_compensation_cash_only_news(
    const KboFaCompensationRecord* rec,
    uint32_t decided_yyyymmdd)
{
    if (rec == NULL || rec->player_id == 0u || decided_yyyymmdd == 0u) {
        return;
    }

    uint32_t year = decided_yyyymmdd / 10000u;
    uint32_t month = (decided_yyyymmdd / 100u) % 100u;
    uint32_t day = decided_yyyymmdd % 100u;
    if (year < KBO_SEASON_YEAR_MIN || month == 0u || day == 0u) {
        return;
    }

    char cash_text[32] = "-";
    kbo_fa_compensation_format_salary_text((int32_t)rec->cash_only, cash_text, sizeof(cash_text));

    char fa_player_link[144] = {0};
    char signing_team_link[96] = {0};
    char original_team_link[96] = {0};
    snprintf(
        fa_player_link,
        sizeof(fa_player_link),
        "<%s:player#%u>",
        rec->player_name[0] != '\0' ? rec->player_name : "FA player",
        rec->player_id);
    kbo_fa_compensation_copy_team_link(rec->signing_team_id, signing_team_link, sizeof(signing_team_link));
    kbo_fa_compensation_copy_team_link(rec->original_team_id, original_team_link, sizeof(original_team_link));

    KboNewsTemplateVar news_vars[] = {
        { "player_name", rec->player_name[0] != '\0' ? rec->player_name : "FA player" },
        { "fa_player_link", fa_player_link },
        { "original_team_link", original_team_link },
        { "signing_team_link", signing_team_link },
        { "grade", rec->grade },
        { "cash_text", cash_text },
    };

    const char* title_key = rec->requires_player_compensation
        ? "fa_compensation.cash_only.title"
        : "fa_compensation.cash_required.title";
    const char* body_key = rec->requires_player_compensation
        ? "fa_compensation.cash_only.body"
        : "fa_compensation.cash_required.body";
    const char* log_source = rec->requires_player_compensation
        ? "fa_compensation_cash"
        : "fa_compensation_cash_required";

    char title[160] = {0};
    char body[1200] = {0};
    if (!kbo_news_template_render_key(
            title_key,
            news_vars,
            (int)(sizeof(news_vars) / sizeof(news_vars[0])),
            title,
            sizeof(title),
            log_source)
            || !kbo_news_template_render_key(
                body_key,
                news_vars,
                (int)(sizeof(news_vars) / sizeof(news_vars[0])),
                body,
                sizeof(body),
                log_source)) {
        kbo_log_runtimef(
            "KBO FA compensation cash-only news skipped fa_player=%u reason=template_unavailable",
            rec->player_id);
        return;
    }

    create_kbo_native_live_news_with_body(year, month, day, rec->league_id, 10u, title, body);
}
