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
    if (year < 1982u || month == 0u || day == 0u) {
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
    if (year < 1982u || month == 0u || day == 0u) {
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
    if (year < 1982u || month == 0u || day == 0u) {
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

    const char* fa_player_name = rec->player_name[0] != '\0' ? rec->player_name : "FA signing";
    char signing_team_id_text[16] = {0};
    char original_team_id_text[16] = {0};
    snprintf(signing_team_id_text, sizeof(signing_team_id_text), "%u", rec->signing_team_id);
    snprintf(original_team_id_text, sizeof(original_team_id_text), "%u", rec->original_team_id);

    KboNewsTemplateVar history_vars[] = {
        { "fa_player_name", fa_player_name },
        { "signing_team_id", signing_team_id_text },
        { "original_team_id", original_team_id_text },
        { "cash_text", cash_text },
    };
    char history_text[512] = {0};
    if (kbo_news_template_render_key(
            "fa_compensation.history.selected",
            history_vars,
            (int)(sizeof(history_vars) / sizeof(history_vars[0])),
            history_text,
            sizeof(history_text),
            "fa_compensation_player")) {
        insert_kbo_player_history_sql(selected->player_id, year, month, day, history_text, "fa_compensation_player");
    }

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

int kbo_transfer_fa_compensation_player_to_original_team(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    uint32_t transfer_yyyymmdd,
    const char* source)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u || rec->original_team_id == 0u) {
        return 0;
    }

    uint32_t before_current_team = 0u;
    uint32_t before_current_league = 0u;
    uint8_t* player = kbo_find_player_by_id(selected->player_id, &before_current_team, &before_current_league);
    uint8_t* destination_team = find_kbo_team_by_numeric_id_any_league(rec->original_team_id, 1);
    if (player == NULL || destination_team == NULL) {
        kbo_log_runtimef(
            "KBO FA compensation transfer failed reason=lookup fa_player=%u selected=%u player=%p team=%p original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            player,
            destination_team,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    uint32_t before_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (before_current_team != rec->signing_team_id && before_active_team != rec->signing_team_id) {
        kbo_log_runtimef(
            "KBO FA compensation transfer skipped reason=player_not_on_signing_team fa_player=%u selected=%u current=%u active=%u signing_team=%u original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            before_current_team,
            before_active_team,
            rec->signing_team_id,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    int called_pre_change = 0;
    int called_register = 0;
    int called_attach = 0;
    kbo_assign_player_to_team_like_ootp(
        player,
        destination_team,
        rec->league_id,
        &called_pre_change,
        &called_register,
        &called_attach);

    uint32_t after_current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (after_current_team != rec->original_team_id && after_active_team != rec->original_team_id) {
        kbo_log_runtimef(
            "KBO FA compensation transfer failed reason=post_verify fa_player=%u selected=%u current=%u->%u active=%u->%u original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            before_current_team,
            after_current_team,
            before_active_team,
            after_active_team,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    int year = (int)(transfer_yyyymmdd / 10000u);
    int month = (int)((transfer_yyyymmdd / 100u) % 100u);
    int day = (int)(transfer_yyyymmdd % 100u);
    char original_team_id_text[16] = {0};
    snprintf(original_team_id_text, sizeof(original_team_id_text), "%u", rec->original_team_id);
    KboNewsTemplateVar history_vars[] = {
        { "original_team_id", original_team_id_text },
        { "fa_player_name", rec->player_name[0] != '\0' ? rec->player_name : "the signing" },
    };
    char history_text[256] = {0};
    if (kbo_news_template_render_key(
            "fa_compensation.history.transfer",
            history_vars,
            (int)(sizeof(history_vars) / sizeof(history_vars[0])),
            history_text,
            sizeof(history_text),
            source)) {
        insert_kbo_player_history_sql(selected->player_id, year, month, day, history_text, "fa_compensation_transfer");
    }

    kbo_log_runtimef(
        "KBO FA compensation player transferred fa_player=%u selected=%u name=%s signing_team=%u original_team=%u current=%u->%u active=%u->%u pre=%d register=%d attach=%d source=%s",
        rec->player_id,
        selected->player_id,
        selected->player_name,
        rec->signing_team_id,
        rec->original_team_id,
        before_current_team,
        after_current_team,
        before_active_team,
        after_active_team,
        called_pre_change,
        called_register,
        called_attach,
        source != NULL ? source : "");
    return 1;
}
