#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/news/live/core_live_news.h"
#include "../../core/news/templates/core_news_templates.h"
#include "../../core/logging/core_log.h"
#include "../../core/sql/history_transactions/core_sql_history_transactions.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/assignment/assignment/team_assignment.h"
#include "../../team/lookup/team_lookup.h"
#include "fa_compensation_news_transfer.h"
#include "fa_compensation_news_transfer_internal.h"
#include "../../core/dates/constants/kbo_date_constants.h"

void kbo_emit_fa_compensation_obligation_news(
    const KboFaCompensationRecord* rec,
    uint32_t event_yyyymmdd,
    uint32_t protected_list_due_days)
{
    if (rec == NULL || rec->player_id == 0u || event_yyyymmdd == 0u
            || !rec->requires_player_compensation) {
        return;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    if (year < KBO_SEASON_YEAR_MIN || month == 0u || day == 0u) {
        return;
    }

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

    char cash_with_player_text[32] = "-";
    char cash_only_text[32] = "-";
    char protect_count_text[16] = {0};
    char protected_list_due_days_text[16] = {0};
    kbo_fa_compensation_format_salary_text((int32_t)rec->cash_with_player, cash_with_player_text, sizeof(cash_with_player_text));
    kbo_fa_compensation_format_salary_text((int32_t)rec->cash_only, cash_only_text, sizeof(cash_only_text));
    snprintf(protect_count_text, sizeof(protect_count_text), "%u", rec->protect_count);
    snprintf(protected_list_due_days_text, sizeof(protected_list_due_days_text), "%u", protected_list_due_days);

    KboNewsTemplateVar news_vars[] = {
        { "player_name", rec->player_name[0] != '\0' ? rec->player_name : "FA player" },
        { "fa_player_link", fa_player_link },
        { "original_team_link", original_team_link },
        { "signing_team_link", signing_team_link },
        { "grade", rec->grade },
        { "cash_with_player_text", cash_with_player_text },
        { "cash_only_text", cash_only_text },
        { "protect_count", protect_count_text },
        { "protected_list_due_days", protected_list_due_days_text },
    };

    char title[180] = {0};
    char body[1400] = {0};
    if (!kbo_news_template_render_key(
            "fa_compensation.obligation.title",
            news_vars,
            (int)(sizeof(news_vars) / sizeof(news_vars[0])),
            title,
            sizeof(title),
            "fa_compensation_obligation")
            || !kbo_news_template_render_key(
                "fa_compensation.obligation.body",
                news_vars,
                (int)(sizeof(news_vars) / sizeof(news_vars[0])),
                body,
                sizeof(body),
                "fa_compensation_obligation")) {
        kbo_log_runtimef(
            "KBO FA compensation obligation news skipped fa_player=%u reason=template_unavailable",
            rec->player_id);
        return;
    }

    create_kbo_native_live_news_with_body(year, month, day, rec->league_id, 10u, title, body);
}

void kbo_emit_fa_compensation_protected_list_submitted_news(
    const KboFaCompensationRecord* rec,
    uint32_t generated_yyyymmdd,
    uint32_t selection_due_days,
    int protected_count,
    int unprotected_count)
{
    if (rec == NULL || rec->player_id == 0u || generated_yyyymmdd == 0u
            || !rec->requires_player_compensation) {
        return;
    }

    uint32_t year = generated_yyyymmdd / 10000u;
    uint32_t month = (generated_yyyymmdd / 100u) % 100u;
    uint32_t day = generated_yyyymmdd % 100u;
    if (year < KBO_SEASON_YEAR_MIN || month == 0u || day == 0u) {
        return;
    }

    if (protected_count < 0) {
        protected_count = 0;
    }
    if (unprotected_count < 0) {
        unprotected_count = 0;
    }

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

    char cash_only_text[32] = "-";
    char protected_count_text[16] = {0};
    char unprotected_count_text[16] = {0};
    char selection_due_days_text[16] = {0};
    kbo_fa_compensation_format_salary_text((int32_t)rec->cash_only, cash_only_text, sizeof(cash_only_text));
    snprintf(protected_count_text, sizeof(protected_count_text), "%d", protected_count);
    snprintf(unprotected_count_text, sizeof(unprotected_count_text), "%d", unprotected_count);
    snprintf(selection_due_days_text, sizeof(selection_due_days_text), "%u", selection_due_days);

    KboNewsTemplateVar news_vars[] = {
        { "player_name", rec->player_name[0] != '\0' ? rec->player_name : "FA player" },
        { "fa_player_link", fa_player_link },
        { "original_team_link", original_team_link },
        { "signing_team_link", signing_team_link },
        { "grade", rec->grade },
        { "cash_only_text", cash_only_text },
        { "protected_count", protected_count_text },
        { "unprotected_count", unprotected_count_text },
        { "selection_due_days", selection_due_days_text },
    };

    char title[180] = {0};
    char body[1400] = {0};
    if (!kbo_news_template_render_key(
            "fa_compensation.protected_list_submitted.title",
            news_vars,
            (int)(sizeof(news_vars) / sizeof(news_vars[0])),
            title,
            sizeof(title),
            "fa_compensation_protected_list")
            || !kbo_news_template_render_key(
                "fa_compensation.protected_list_submitted.body",
                news_vars,
                (int)(sizeof(news_vars) / sizeof(news_vars[0])),
                body,
                sizeof(body),
                "fa_compensation_protected_list")) {
        kbo_log_runtimef(
            "KBO FA compensation protected-list news skipped fa_player=%u reason=template_unavailable",
            rec->player_id);
        return;
    }

    create_kbo_native_live_news_with_body(year, month, day, rec->league_id, 10u, title, body);
}

void kbo_emit_fa_compensation_player_selected_news(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    uint32_t decided_yyyymmdd)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u || decided_yyyymmdd == 0u) {
        return;
    }

    uint32_t year = decided_yyyymmdd / 10000u;
    uint32_t month = (decided_yyyymmdd / 100u) % 100u;
    uint32_t day = decided_yyyymmdd % 100u;
    if (year < KBO_SEASON_YEAR_MIN || month == 0u || day == 0u) {
        return;
    }

    char cash_text[32] = "-";
    kbo_fa_compensation_format_salary_text((int32_t)rec->cash_with_player, cash_text, sizeof(cash_text));

    char selected_name[96] = {0};
    snprintf(
        selected_name,
        sizeof(selected_name),
        "%s",
        selected->player_name[0] != '\0' ? selected->player_name : "Compensation player");

    const char* fa_player_name = rec->player_name[0] != '\0' ? rec->player_name : "the FA signing";
    char signing_team_name[96] = {0};
    char original_team_name[96] = {0};
    char history_text[512] = {0};
    kbo_fa_compensation_copy_team_history_name(rec->signing_team_id, signing_team_name, sizeof(signing_team_name));
    kbo_fa_compensation_copy_team_history_name(rec->original_team_id, original_team_name, sizeof(original_team_name));
    snprintf(
        history_text,
        sizeof(history_text),
        "[G]Selected as the KBO FA compensation player for %s. Rights transferred from %s to %s, with %s cash compensation recorded.",
        fa_player_name,
        signing_team_name[0] != '\0' ? signing_team_name : "his previous KBO organization",
        original_team_name[0] != '\0' ? original_team_name : "his new KBO organization",
        cash_text);
    int history_recorded = insert_kbo_player_history_sql(
        selected->player_id,
        year,
        month,
        day,
        history_text,
        "fa_compensation_player");
    kbo_log_runtimef(
        "KBO FA compensation selected player history source=fa_compensation_player fa_player=%u selected=%u date=%u signing_team=%u original_team=%u recorded=%d",
        rec->player_id,
        selected->player_id,
        decided_yyyymmdd,
        rec->signing_team_id,
        rec->original_team_id,
        history_recorded);

    char title[160] = {0};
    char body[1200] = {0};
    char selected_player_link[144] = {0};
    char fa_player_link[144] = {0};
    char signing_team_link[96] = {0};
    char original_team_link[96] = {0};
    snprintf(selected_player_link, sizeof(selected_player_link), "<%s:player#%u>", selected_name, selected->player_id);
    snprintf(fa_player_link, sizeof(fa_player_link), "<%s:player#%u>", rec->player_name[0] != '\0' ? rec->player_name : "FA player", rec->player_id);
    kbo_fa_compensation_copy_team_link(rec->signing_team_id, signing_team_link, sizeof(signing_team_link));
    kbo_fa_compensation_copy_team_link(rec->original_team_id, original_team_link, sizeof(original_team_link));
    KboNewsTemplateVar news_vars[] = {
        { "selected_name", selected_name },
        { "selected_player_link", selected_player_link },
        { "original_team_link", original_team_link },
        { "grade", rec->grade },
        { "fa_player_link", fa_player_link },
        { "signing_team_link", signing_team_link },
        { "cash_text", cash_text },
    };
    if (!kbo_news_template_render_key(
            "fa_compensation.player_selected.title",
            news_vars,
            (int)(sizeof(news_vars) / sizeof(news_vars[0])),
            title,
            sizeof(title),
            "fa_compensation_player")
            || !kbo_news_template_render_key(
                "fa_compensation.player_selected.body",
                news_vars,
                (int)(sizeof(news_vars) / sizeof(news_vars[0])),
                body,
                sizeof(body),
                "fa_compensation_player")) {
        kbo_log_runtimef(
            "KBO FA compensation news skipped selected=%u fa_player=%u reason=template_unavailable",
            selected->player_id,
            rec->player_id);
        return;
    }

    create_kbo_native_live_news_with_body(year, month, day, rec->league_id, 10u, title, body);
}
